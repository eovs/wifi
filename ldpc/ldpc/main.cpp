#include "stdafx.h"
#include <iostream>
#include <vector>

#include "../../QA_lib/LDPC_dec_engine.h"
#include "../../SOURCE/matrix.h"
#include "../../SOURCE/decoders.h"
#include "../../SOURCE/encode_qc.h"
#include "../../SOURCE/modulation.h"

//#define USE_ANCHOR_DECODER

#define N_SNR	100
#define TARGET_FER 0.0001
#define N_ITER	15
#define N_EVENT	10//25
#define NPASS	1000000

#define CFG     2
//#define NO_MODULATION	

#ifndef NO_MODULATION
#define CODE_M	27
#endif

#define DEC_TYPE	8//3, 4, 8, 9

using namespace std;

int QA_decoder( ldpc_dec_engine_t &dec, int n_iter );

double next_random_01() {
	//ensure_random_is_initialized();

	int bits_per_random = 0;
	for (int v = RAND_MAX; v > 0; v >>= 1, ++bits_per_random);

	// Collecting enough digits...
	int required_bits = 54;
	int collected_bits = 0;
	long long collected = 0;
	while (collected_bits < required_bits) {
		int value = rand();
		int required_diff = required_bits - collected_bits;
		if (required_diff >= bits_per_random) {
			collected = (collected << bits_per_random) + value;
			collected_bits += bits_per_random;
		}
		else {
			collected = (collected << required_diff) + (value >> (bits_per_random - required_diff));
			collected_bits += required_diff;
		}
	}
	//if (collected < 0 || collected >= (1LL << required_bits)) {
	//	die("next_random_01 [C++98] does not work properly");
	//}
	return (double)(collected) / (double)(1LL << required_bits);
}

double next_random_gaussian() {
	//ensure_random_is_initialized();
	static bool has_next_next = false;
	static double next_next = 0;

	if (has_next_next) {
		has_next_next = false;
		return next_next;
	}
	else {
		double u, v, w;
		do {
			u = 2.0 * next_random_01() - 1.0;
			v = 2.0 * next_random_01() - 1.0;
			w = u * u + v * v;
		} while (w >= 1.0);
		w = sqrt(-2 * log(w) / w);
		next_next = v * w;
		has_next_next = true;
		return u * w;
	}
}


typedef struct
{
	double start_snr;
	double stop_snr;
	double step_snr;
} SNR_CFG;

int main( void ) 
{
	int i, j;
	int pass;
	int n_pass;
	int codelen;
	ldpc_dec_engine_t dec_engine;
	vector<int>in;
	double snr;
	int iter;
	double bitrate;
	double sigma;
	int qa_dec_OK = 1;
	int itmo_dec_OK = 1;
	int nerr;
	int qaBER;
	int qaFER;
	int itmoBER;
	int itmoFER;
	int cfg;
	int *matr_ptr;
	DEC_STATE *dec_state = NULL;
	QAM_MODULATOR_STATE* qam_mod_st = NULL;	
	QAM_DEMODULATOR_STATE* qam_demod_st = NULL;	

	int decoder_type;
	int out_type;
	int n_iter;
	CODE_CFG code;
	SNR_CFG snr_cfg;
	int *codeword;
	double *ch_buf;
	double *dm_buf;
	int *mod_buf;
	int inflen;
	int snr_ind;
	int QAM;
	int halfmlog;
	int nQAMSignals;
	int irate;
	double norm_factor;
	int bps;	// bits per signal
	double T = 26.0;	//?????????????????????????????????????????????

	double QA_FER[N_SNR];
	double ITMO_FER[N_SNR];
	FILE *fp;

	cfg = CFG;
#ifdef NO_MODULATION 
	switch (cfg)
	{
	case  0: code = { 12, 24, 27, (int*)H1x2m27 }; snr_cfg = {2.0, 3.0, 0.25 };	break;
	case  1: code = {  8, 24, 27, (int*)H2x3m27 }; snr_cfg = {2.0, 3.0, 0.25 };	break;
	case  2: code = {  6, 24, 27, (int*)H3x4m27 }; snr_cfg = {3.0, 5.0, 0.25 };	break;
	case  3: code = {  4, 24, 27, (int*)H5x6m27 }; snr_cfg = {2.0, 3.0, 0.25 };	break;

	case  4: code = { 12, 24, 54, (int*)H1x2m54 }; snr_cfg = {2.0, 3.0, 0.25 };	break;
	case  5: code = {  8, 24, 54, (int*)H2x3m54 }; snr_cfg = {2.0, 3.0, 0.25 };	break;
	case  6: code = {  6, 24, 54, (int*)H3x4m54 }; snr_cfg = {2.0, 3.0, 0.25 };	break;
	case  7: code = {  4, 24, 54, (int*)H5x6m54 }; snr_cfg = {2.0, 3.0, 0.25 };	break;

	case  8: code = { 12, 24, 81, (int*)H1x2m81 }; snr_cfg = {1.5, 3.0, 0.25 };	break;
	case  9: code = {  8, 24, 81, (int*)H2x3m81 }; snr_cfg = {2.0, 3.0, 0.25 };	break;
	case 10: code = {  6, 24, 81, (int*)H3x4m81 }; snr_cfg = {2.5, 5.0, 0.25 };	break;
	case 11: code = {  4, 24, 81, (int*)H5x6m81 }; snr_cfg = {2.0, 3.0, 0.25 };	break;

	default: code = { 12, 24, 27, (int*)H1x2m27 }; snr_cfg = {2.0, 3.0, 0.25 };	break;
	};
	QAM = 0;
#else

	switch (cfg)
	{
	case  1: QAM =   2; irate = 1; snr_cfg = { 1.0,  3.0, 0.25 };	break;	// BPSK; R=1/2
	case  2: QAM =   4; irate = 1; snr_cfg = { 1.0,  3.0, 0.25 };	break;	// QPSK; R=1/2
	case  3: QAM =   4; irate = 3; snr_cfg = { 2.0,  4.0, 0.25 };	break;	// QPSK; R=3/4
	case  4: QAM =  16; irate = 1; snr_cfg = { 3.0,  6.0, 0.25 };	break;	// QPSK; R=1/2
	case  5: QAM =  16; irate = 3; snr_cfg = { 4.5,  7.5, 0.25 };	break;	// QPSK; R=3/4
	case  6: QAM =  64; irate = 2; snr_cfg = { 7.0, 10.0, 0.25 };	break;	// QPSK; R=2/3
	case  7: QAM =  64; irate = 3; snr_cfg = { 8.5, 11.0, 0.25 };	break;	// QPSK; R=3/4
	case  8: QAM =  64; irate = 4; snr_cfg = { 9.5, 12.0, 0.25 };	break;	// QPSK; R=5/6
	case  9: QAM = 256; irate = 3; snr_cfg = {12.0, 15.0, 0.25 };	break;	// QPSK; R=3/4
	case 10: QAM = 256; irate = 4; snr_cfg = {13.5, 16.5, 0.25 };	break;	// QPSK; R=5/6

	default: QAM =   2, irate =1; snr_cfg = {1.0, 3.0, 0.25 };	break;	// BPSK; R=1/2
	};

	code = select_code( irate, CODE_M );

#endif


	switch( QAM )
	{
	case 1:		bps = 1; halfmlog = 1;	break;
	case 2:		bps = 1; halfmlog = 1;	 break;
	case 4:		bps = 2; halfmlog = 1;	break;
	case 16:	bps = 4; halfmlog = 2;	break;
	case 64:	bps = 6; halfmlog = 3;	break;
	case 256:	bps = 8; halfmlog = 4;	break;
	};

	vector< vector<int> > check_matrix(code.nrow, vector<int>(code.ncol, 0));

	n_pass = NPASS;

	// initialize check matrix
	matr_ptr = code.matrix;
	for( i = 0; i < code.nrow; i++ )
		for( j = 0; j < code.ncol; j++ )
			check_matrix[i][j] = *matr_ptr++;


	codelen = code.M * code.ncol;
	nQAMSignals = codelen / bps;

  	n_iter = N_ITER;

	vector<int> llr(codelen);

	
	decoder_type = DEC_TYPE;

	if( dec_state )
		decod_close( dec_state );

	dec_state = decod_open( decoder_type, 1, code.nrow, code.ncol, code.M );
	if (!dec_state)
	{
		printf("Can't open decode\n");
		return 0;
	}

	matr_ptr = code.matrix;
	for( i = 0; i < code.nrow; i++ )
		for( j = 0; j < code.ncol; j++ )
			dec_state->hd[i][j] = *matr_ptr++;

	// initialize decoder
	decod_init(dec_state);

	bitrate = (double)(code.ncol - code.nrow) / code.ncol;
	inflen  = (code.ncol - code.nrow) * code.M;

	codeword = (int*)malloc( codelen * sizeof(codeword[0]) );
	mod_buf = (int*)malloc( codelen * sizeof(mod_buf[0]) );
	ch_buf = (double*)malloc( codelen * sizeof(ch_buf[0]) );
	dm_buf = (double*)malloc( codelen * sizeof(dm_buf[0]) );

	if( qam_mod_st )
		QAM_modulator_close(qam_mod_st);
	qam_mod_st = QAM_modulator_open( QAM, codelen, bps );

	if (!qam_mod_st)
	{
		printf( "Can't open QAM modulator" );
		return 0;
	}


#if 01
	for( i = 0; i < inflen; i++ )
		codeword[i] = rand() % 2;
#else
	for( i = 0; i < inflen; i++ )
		codeword[i] = 0;
	codeword[0] = 1;
#endif

	encode_qc( codeword, dec_state->hd, code.M, code.nrow, code.ncol, dec_state->syndr );

	// initialize QA decoder
	dec_engine.init(check_matrix, code.M);

	if( QAM > 2 )
		QAM_modulator( qam_mod_st, codeword, mod_buf  );

    switch( decoder_type )
    {
    case BP_DEC:    out_type = 0;		break;
    case SP_DEC:    out_type = 1;		break;
    case ASP_DEC:   out_type = 1;		break;
    case MS_DEC:    out_type = 0;		break;
    case IMS_DEC:   out_type = 0;		break;
    case IASP_DEC:  out_type = 1;		break;
	case TASP_DEC:  out_type = 1;		break;
	case LMS_DEC:   out_type = 0;		break;
	case LCHE_DEC:  out_type = 1;		break;
    default: out_type = -1; printf("Unknown decoder type: %d\n", decoder_type); return 0;
    }


	snr_ind = 0;
	for( snr = snr_cfg.start_snr; snr <= snr_cfg.stop_snr; snr += snr_cfg.step_snr )
	{
		printf("\n=======================\n");
		//sigma = sqrt(pow(10, -snr / 10) / 2 / bitrate);

		norm_factor = QAM > 2 ? 2*(QAM-1)/3 : 1;

		sigma = sqrt( pow(10, -snr / 10) / (2 * bitrate *bps) * norm_factor );

		printf("Eb/No = %f, sigma = %f\n", snr, sigma );

		qaBER = 0;
		qaFER = 0;
		itmoBER = 0;
		itmoFER = 0;
		

		if( qam_demod_st )
			QAM_demodulator_close(qam_demod_st );
		qam_demod_st = QAM_demodulator_open( T, sigma, QAM, codelen, bps, nQAMSignals, out_type );
		if( !qam_demod_st )
		{
			printf("Can't open QAM demodulator");
			return 0;
		}

		for( pass = 0; pass < n_pass; pass++ )
		{
			// add noice
			if( QAM > 2 )
			{
				for( i = 0; i < codelen; i++ )
				{
					double noise = next_random_gaussian();
					ch_buf[i] = noise * sigma + mod_buf[i];
				}

				Demodulate( qam_demod_st, ch_buf, dm_buf );
				
				for( i = 0; i < codelen; i++ )	dm_buf[i] = -dm_buf[i] ;

				for( i = 0; i < codelen; i++ )
				{					
					double real_llr = dm_buf[i];
					double abs_llr = real_llr < 0.0 ? -real_llr : real_llr;
					int sign = real_llr < 0.0 ? 1 : 0;
					int int_llr = (int)(abs_llr + 0.5);
					int_llr = int_llr <  63 ? int_llr :  63;

					llr[i] = sign == 0 ? int_llr : -int_llr;
			
					dec_state->y[i] = llr[i];
				}
			}
			else
			{
				for( i = 0; i < codelen; i++ )
				{
					double noise = next_random_gaussian();
					double real_llr = -2.0 * (sigma * noise + (2.0 * codeword[i] - 1.0) ) / (sigma * sigma);
					double abs_llr = real_llr < 0.0 ? -real_llr : real_llr;
					int sign = real_llr < 0.0 ? 1 : 0;
					int int_llr = (int)(abs_llr + 0.5);

					int_llr = int_llr <  63 ? int_llr :  63;

					llr[i] = sign == 0 ? int_llr : -int_llr;
			
					dec_state->y[i] = llr[i];
				}
			}

#ifdef USE_ANCHOR_DECODER
			// reset decoder
			dec_engine.reset();

			// load input LLRs
			dec_engine.push(llr);

			iter = QA_decoder( dec_engine, n_iter );

			auto dec_cw = dec_engine.pull();
			// process decoded sequence

			for( i = 0, nerr = 0; i < codelen; i++ )
			{
				if( codeword[i] != (int)dec_cw[i] )
					nerr++;
			}
			qa_dec_OK = nerr == 0 ? 1 : 0;

//			if( iter < 0 )
//				qa_dec_OK = 0;

			qaBER += nerr;
			qaFER += qa_dec_OK == 0;
#endif
		//	printf( "QA iterations: %5d, result: %s\n", iter < n_iter ? iter + 1 : iter, dec_OK ? "OK" : "ERROR" );


			//for( i = 0; i < codelen; i++ )	dec_state->decword[i] = 0;


			switch( decoder_type )
			{
			case BP_DEC:    iter = bp_decod_qc_lm( dec_state, dec_state->y, dec_state->decword, n_iter, DEC_DECISION);				 break;
			case SP_DEC:    iter = sum_prod_decod_qc_lm( dec_state, dec_state->y, dec_state->decword, n_iter, DEC_DECISION);			 break;
			case ASP_DEC:   iter = sum_prod_gf2_decod_qc_lm( dec_state, dec_state->y, dec_state->decword, n_iter, DEC_DECISION);		 break;
			case MS_DEC:    iter = min_sum_decod_qc_lm( dec_state, dec_state->y, dec_state->decword, n_iter, DEC_DECISION, MS_ALPHA); break;
			case IMS_DEC:   iter = imin_sum_decod_qc_lm( dec_state, dec_state->y, dec_state->decword, n_iter, DEC_DECISION, MS_ALPHA, MS_THR, MS_QBITS, MS_DBITS); break;
			case IASP_DEC:  iter = isum_prod_gf2_decod_qc_lm( dec_state, dec_state->y, dec_state->decword, n_iter, DEC_DECISION);		 break;
			case TASP_DEC:  iter = tdmp_sum_prod_gf2_decod_qc_lm( dec_state, dec_state->y, dec_state->decword, n_iter, DEC_DECISION);		 break;
			case LMS_DEC:   iter = lmin_sum_decod_qc_lm( dec_state, dec_state->y, dec_state->decword, n_iter, DEC_DECISION, MS_ALPHA, MS_BETA); break;
			case LCHE_DEC:  iter = lche_decod( dec_state, dec_state->y, dec_state->decword, n_iter, DEC_DECISION);		 break;
			default: iter = -1; printf("Unknown decoder type: %d\n", decoder_type); return 0;
			}


	  		for( i = 0, nerr = 0; i < codelen; i++ )
			{
				if( codeword[i] != dec_state->decword[i] )
					nerr++;
			}
			itmo_dec_OK = nerr == 0 ? 1 : 0;

//			if( iter < 0 )
//				dec_OK = 0;

			itmoBER += nerr;
			itmoFER += itmo_dec_OK == 0;

//			printf( "ITMO iterations: %5d, result: %s\n", iter, dec_OK ? "OK" : "ERROR" );


//			printf( "QA fer: %6.4f |  ", (double)qaFER   / (pass+1) );
//			printf( "ITMO fer: %6.4f ", (double)itmoFER / (pass+1) );
			if( (pass % 1000 == 0) || itmo_dec_OK == 0 || qa_dec_OK == 0)
			{
				printf("pass #%6d ", pass);
				printf( "QA fer: %6d |  ", qaFER );
				printf( "ITMO fer: %6d ", itmoFER );
				printf("\r");
			}

			if( itmoFER == N_EVENT )
				break;

		}
		

		printf("\nnumber of passes = %d\n", pass );

		printf("\n");
		printf( "QA   pass: %5d, n_ber: %8d, n_fer: %5d\n", pass, qaBER, qaFER );
		printf( "ITMO pass: %5d, n_ber: %8d, n_fer: %5d\n", pass, itmoBER, itmoFER );
		printf("\n");
		printf( "QA   pass: %5d, BER: %10.8f, FER: %10.8f\n", pass, (double)qaBER/pass/codelen, (double)qaFER/pass );
		printf( "ITMO pass: %5d, BER: %10.8f, FER: %10.8f\n", pass, (double)itmoBER/pass/codelen, (double)itmoFER/pass );
	

		fopen_s( &fp, "result.txt", "at" );
		if( fp == NULL )
		{
			printf("file error\n");
			fclose(fp);
		}
		else
		{
			if (snr_ind == 0)
				fprintf( fp, "\n\ncode: %d x %d x %d\n", code.nrow, code.ncol, code.M );

			fprintf( fp, "Eb/No = %f, sigma = %f ", snr, sigma );
			fprintf( fp, "QA_FER:   %10.8f, ", (double)qaFER / pass );
			fprintf( fp, "ITMO_FER: %10.8f\n", (double)itmoFER / pass );

			fclose( fp );
		}

		QA_FER[snr_ind]   = (double)qaFER / pass;
		ITMO_FER[snr_ind] = (double)itmoFER / pass;
		snr_ind++;


		//if( (double)itmoFER/pass < TARGET_FER ) 	break;
	}

	QAM_modulator_close( qam_mod_st);

	free( codeword );
	free( dm_buf );
	free( ch_buf );
	free( mod_buf );

	return 0;
}

int QA_decoder( ldpc_dec_engine_t &dec, int n_iter )
{
	int iter;
		
	for( iter = 0; iter < n_iter; iter++ )
	{
		auto ret = dec.iterate();
		auto parity = dec.calc_parity_check();
//			cout << "iteration output: " << (int)ret << "\n";
//			cout << "final check is : " << (int)parity << "\n";
		if (!parity) break;
	}

	if( iter == n_iter )
		iter = -iter;

	return iter;
}