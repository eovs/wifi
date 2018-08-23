#include <iostream>
#include <vector>
#include <math.h>

#include "../SOURCE/params.h"
#include "../SOURCE/matrix.h"
#include "../SOURCE/decoders.h"
#include "../SOURCE/encode_qc.h"
#include "../SOURCE/modulation.h"

#include "../QANT_LIB/LDPC_dec_engine.h"
#include "../SOURCE/itmo_LDPC_dec_engine.h"

//#define USE_ANCHOR_DECODER
//#define USE_MY_DECODER
#define USE_ITMO_DECODER

#define LLR_BITS	6//6
#define N_SNR	100
#define TARGET_FER 0.0001
#define N_ITER	10
#define N_EVENT	10//25
#define N_PASS	1000000
#define CFG     9
//#define NO_MODULATION	

#ifndef NO_MODULATION
#define CODE_M	81
#endif

#define DEC_TYPE	 3// 0 - LMS, 1 - ILMS, 2 - LSP, 3 - ILSP   

#define NORM_LLR 0

#define MS_ALPHA 0.8

using namespace std;

int QANT_decoder( ldpc_dec_engine_t &dec, int n_iter );
int ITMO_decoder( itmo_ldpc_dec_engine_t &dec, int n_iter );

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

void normalize( double x[], int n, double maxlevel )
{
	int i;
	double coef;
	double maxval = fabs( x[0] );

	for( i = 0; i < n; i++ )
	{
		double absval = fabs( x[i] );
		if( maxval < absval )
			maxval = absval;
	}

	coef = maxlevel / maxval;
	for( i = 0; i < n; i++ )
		x[i] = x[i] * coef;
}

typedef struct
{
	double start_snr;
	double stop_snr;
	double step_snr;
} SNR_CFG;

int main( int argc, char *argv[] ) 
{
	int i, j;
	int pass;
	int codelen;
#ifdef USE_ANCHOR_DECODER
	ldpc_dec_engine_t dec_engine;
#endif
#ifdef USE_ITMO_DECODER
	itmo_ldpc_dec_engine_t itmo_dec_engine;
#endif
	vector<int>in;
	double snr;
	int qant_iter;
	int itmo_iter;
	double bitrate;
	double sigma;
	int qant_dec_OK = 1;
	int itmo_dec_OK = 1;
	int nerr;
	int qantBER;
	int qantFER;
	int itmoBER;
	int itmoFER;
	int *matr_ptr;
	DEC_STATE *dec_state = NULL;
	QAM_MODULATOR_STATE* qam_mod_st = NULL;	
	QAM_DEMODULATOR_STATE* qam_demod_st = NULL;	

	int il_inner_data_bits;
	CODE_CFG code;
	SNR_CFG snr_cfg;
	int *codeword;
	int *dec_input;
	int *dec_output;
	double *ch_buf[2];
	double *dm_buf;
	int *QAM_buf[2];
	int inflen;
	int snr_ind;
	int QAM;
	int halfmlog;
	int nQAMSignals;
	int irate;
	double norm_factor;
	int bps;	// bits per signal
	double T = 26.0;	//?????????????????????????????????????????????
	int norm_llr;
	double QANT_FER[N_SNR];
	double ITMO_FER[N_SNR];
	char *fileName;
	char sdtFileName[] = "result.txt";
	SIMULATION_PARAMS params;
	FILE *fp;

	if( argc == 2 )
	{
		fileName = argv[1];
		set_params( fileName, &params );
	}
	else
	{
		fileName = sdtFileName;
		params.cfg = CFG;
		params.code_M = CODE_M;
		params.n_pass = N_PASS;
		params.n_iter = N_ITER;
		params.n_event = N_EVENT;
		params.llr_bits = LLR_BITS;
		params.dec_type = DEC_TYPE;
		params.target_FER = TARGET_FER;
	}

	norm_llr = NORM_LLR;

	il_inner_data_bits = params.llr_bits + 1;

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

	switch( params.cfg )
	{
	case  1: QAM =   2; irate = 1; snr_cfg = { 1.0,  3.0, 0.25 };	break;	// BPSK; R=1/2
	case  2: QAM =   4; irate = 1; snr_cfg = { 1.0,  3.0, 0.25 };	break;	// QPSK; R=1/2
	case  3: QAM =   4; irate = 3; snr_cfg = { 2.0,  4.0, 0.25 };	break;	// QPSK; R=3/4
	case  4: QAM =  16; irate = 1; snr_cfg = { 3.0,  6.0, 0.25 };	break;	// QPSK; R=1/2
	case  5: QAM =  16; irate = 3; snr_cfg = { 4.5,  7.5, 0.25 };	break;	// QPSK; R=3/4
	case  6: QAM =  64; irate = 2; snr_cfg = { 7.0, 10.0, 0.25 };	break;	// QPSK; R=2/3
	case  7: QAM =  64; irate = 3; snr_cfg = { 8.5, 11.0, 0.25 };	break;	// QPSK; R=3/4
	case  8: QAM =  64; irate = 4; snr_cfg = { 9.5, 12.0, 0.25 };	break;	// QPSK; R=5/6
	case  9: QAM = 256; irate = 3; snr_cfg = {12.5, 15.0, 0.25 };	break;	// QPSK; R=3/4
	case 10: QAM = 256; irate = 4; snr_cfg = {13.5, 16.5, 0.25 };	break;	// QPSK; R=5/6

	default: QAM =   2, irate =1; snr_cfg = {1.0, 3.0, 0.25 };	break;	// BPSK; R=1/2
	};

	code = select_code( irate, params.code_M );

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

	// initialize check matrix
	matr_ptr = code.matrix;
	for( i = 0; i < code.nrow; i++ )
		for( j = 0; j < code.ncol; j++ )
			check_matrix[i][j] = *matr_ptr++;


	codelen = code.M * code.ncol;
	nQAMSignals = codelen / bps;


	vector<int> llr(codelen);

	if( dec_state )
		decod_close( dec_state );

	dec_state = decod_open( params.dec_type, 1, code.nrow, code.ncol, code.M );
	if (!dec_state)
	{
		printf("Can't open decode\n");
		return 0;
	}

	matr_ptr = code.matrix;
	for( i = 0; i < code.nrow; i++ )
		for( j = 0; j < code.ncol; j++ )
			dec_state->hd[i][j] = *matr_ptr++;

	bitrate = (double)(code.ncol - code.nrow) / code.ncol;
	inflen  = (code.ncol - code.nrow) * code.M;

	dec_input  = (int*)malloc( codelen * sizeof(dec_input[0]) );
	dec_output = (int*)malloc( codelen * sizeof(dec_output[0]) );
	codeword   = (int*)malloc( codelen * sizeof(codeword[0]) );
	QAM_buf[0] = (int*)malloc( codelen/bps * sizeof(QAM_buf[0][0]) );
	QAM_buf[1] = (int*)malloc( codelen/bps * sizeof(QAM_buf[0][0]) );
	ch_buf[0]  = (double*)malloc( codelen/bps * sizeof(ch_buf[0][0]) );
	ch_buf[1]  = (double*)malloc( codelen/bps * sizeof(ch_buf[0][0]) );
	dm_buf     = (double*)malloc( codelen * sizeof(dm_buf[0]) );

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

	// initialize QANT decoder
#ifdef USE_ANCHOR_DECODER
	dec_engine.init(check_matrix, code.M);
#endif

#ifdef USE_ITMO_DECODER
	itmo_dec_engine.init(check_matrix, code.M);
#endif

	if( QAM > 2 )
		QAM_modulator( qam_mod_st, codeword, QAM_buf  );

 	snr_ind = 0;
	for( snr = snr_cfg.start_snr; snr <= snr_cfg.stop_snr; snr += snr_cfg.step_snr )
	{
		printf("\n=======================\n");
		//sigma = sqrt(pow(10, -snr / 10) / 2 / bitrate);

		norm_factor = QAM > 2 ? 2*(QAM-1)/3 : 1;

		sigma = sqrt( pow(10, -snr / 10) / (2 * bitrate *bps) * norm_factor );

		printf("Eb/No = %f, sigma = %f\n", snr, sigma );

		qantBER = 0;
		qantFER = 0;
		itmoBER = 0;
		itmoFER = 0;
		

		if( qam_demod_st )
			QAM_demodulator_close(qam_demod_st );
		qam_demod_st = QAM_demodulator_open( T, sigma, QAM, codelen, bps, nQAMSignals, 0 );
		if( !qam_demod_st )
		{
			printf("Can't open QAM demodulator");
			return 0;
		}

		for( pass = 0; pass < params.n_pass; pass++ )
		{
			int max_llr = (1 << (params.llr_bits-1)) - 1;

			// add noice
			if( QAM > 2 )
			{
				for( i = 0; i < codelen/bps; i++ )
				{
					double noise0 = next_random_gaussian();
					double noise1 = next_random_gaussian();
					ch_buf[0][i] = noise0 * sigma + QAM_buf[0][i];
					ch_buf[1][i] = noise1 * sigma + QAM_buf[1][i];
				}

				Demodulate( qam_demod_st, ch_buf, dm_buf );
				
				for( i = 0; i < codelen; i++ )	dm_buf[i] = -dm_buf[i] ;

				for( i = 0; i < codelen; i++ )
				{	
					double real_llr = dm_buf[i];
					double abs_llr = real_llr < 0.0 ? -real_llr : real_llr;
					int sign = real_llr < 0.0 ? 1 : 0;
					int int_llr = (int)(abs_llr + 0.5);
					
					
					
					int_llr = int_llr <  max_llr ? int_llr :  max_llr;

					dec_input[i] = llr[i] = sign == 0 ? int_llr : -int_llr;

				}
			}
			else
			{
				for( i = 0; i < codelen; i++ )
				{
					double noise = next_random_gaussian();

					dm_buf[i] = -2.0 * (sigma * noise + (2.0 * codeword[i] - 1.0) ) / (sigma * sigma);
				}
			}

			if( norm_llr )
				normalize( dm_buf, codelen, max_llr );

			for( i = 0; i < codelen; i++ )
			{
				double real_llr = dm_buf[i];
				double abs_llr = real_llr < 0.0 ? -real_llr : real_llr;
				int sign = real_llr < 0.0 ? 1 : 0;
				int int_llr = (int)(abs_llr + 0.5);

				int_llr = int_llr <  max_llr ? int_llr :  max_llr;
				dec_input[i] = llr[i] = sign == 0 ? int_llr : -int_llr;
			}


#ifdef USE_ANCHOR_DECODER
			// reset decoder
			dec_engine.reset();

			// load input LLRs
			dec_engine.push(llr);

			qant_iter = QANT_decoder( dec_engine, params.n_iter );

			auto dec_cw = dec_engine.pull();
			// process decoded sequence

			for( i = 0, nerr = 0; i < codelen; i++ )
			{
				if( codeword[i] != (int)dec_cw[i] )
					nerr++;
			}
			qant_dec_OK = nerr == 0 ? 1 : 0;
//			if( iter < 0 )
//				qant_dec_OK = 0;

			qantBER += nerr;
			qantFER += qant_dec_OK == 0;

#endif
#ifdef USE_ITMO_DECODER
			itmo_dec_engine.reset();
			itmo_dec_engine.push(llr);
			int itmo_iter1 = ITMO_decoder( itmo_dec_engine, params.n_iter );
			auto itmo_dec_cw = itmo_dec_engine.pull();

			for( i = 0, nerr = 0; i < codelen; i++ )
			{
				if( codeword[i] != (int)itmo_dec_cw[i] )
					nerr++;
			}
			itmo_dec_OK = nerr == 0 ? 1 : 0;

			itmoBER += nerr;
			itmoFER += itmo_dec_OK == 0;
#endif
		//	printf( "QANT iterations: %5d, result: %s\n", iter < params.n_iter ? iter + 1 : iter, dec_OK ? "OK" : "ERROR" );


		//	for( i = 0; i < codelen; i++ )	dec_output[i] = 0;

#ifdef USE_MY_DECODER
			switch( params.dec_type )
			{
			case LMS_DEC:   itmo_iter = lmin_sum_decod_qc_lm( dec_state, dec_input, dec_output, params.n_iter, MS_ALPHA, 0.5 ); break;
			case ILMS_DEC:  itmo_iter = il_min_sum_decod_qc_lm( dec_state, dec_input, dec_output, params.n_iter, MS_ALPHA, 0.5, il_inner_data_bits ); break;
			case LCHE_DEC:  itmo_iter = lche_decod( dec_state, dec_input, dec_output, params.n_iter );		 break;
			case ILCHE_DEC: itmo_iter = ilche_decod( dec_state, dec_input, dec_output, params.n_iter );		 break;
			default: itmo_iter = -1; printf("Unknown decoder type: %d\n", params.dec_type); return 0;
			}


	  		for( i = 0, nerr = 0; i < codelen; i++ )
			{
				if( codeword[i] != dec_output[i] )
					nerr++;
			}
			itmo_dec_OK = nerr == 0 ? 1 : 0;


//			if( iter < 0 )
//				dec_OK = 0;

			itmoBER += nerr;
			itmoFER += itmo_dec_OK == 0;
#endif
//			printf( "ITMO iterations: %5d, result: %s\n", iter, dec_OK ? "OK" : "ERROR" );


//			printf( "QANT fer: %6.4f |  ", (double)qaFER   / (pass+1) );
//			printf( "ITMO fer: %6.4f ", (double)itmoFER / (pass+1) );
			if( (pass % 1000 == 0) || itmo_dec_OK == 0 || qant_dec_OK == 0)
			{
				printf("pass #%6d ", pass);
				printf( "QANT fer: %6d |  ", qantFER );
				printf( "ITMO fer: %6d ", itmoFER );
				printf("\r");
			}

			if( itmoFER == params.n_event ){	pass += 1;	break;	}
#ifndef USE_MY_DECODER
			if( qantFER == params.n_event  ){	pass += 1;	break;	}
#endif
		}
		

		printf("\nnumber of passes = %d\n", pass );

		printf("\n");
		printf( "QANT pass: %5d, n_fer: %8d, n_ber: %5d\n", pass, qantFER, qantBER );
		printf( "ITMO pass: %5d, n_fer: %8d, n_ber: %5d\n", pass, itmoFER, itmoBER );
		printf("\n");
		printf( "QANT pass: %5d, FER: %10.8f, BER: %10.8f\n", pass, (double)qantFER/pass, (double)qantBER/pass/codelen );
		printf( "ITMO pass: %5d, FER: %10.8f, BER: %10.8f\n", pass, (double)itmoFER/pass, (double)itmoBER/pass/codelen );
	

		fopen_s( &fp, fileName, "at" );
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
			fprintf( fp, "QANT_FER:   %10.8f, ", (double)qantFER / pass );
			fprintf( fp, "ITMO_FER: %10.8f\n", (double)itmoFER / pass );

			fclose( fp );
		}

		QANT_FER[snr_ind] = (double)qantFER / pass;
		ITMO_FER[snr_ind] = (double)itmoFER / pass;
		snr_ind++;


		if( qantFER > 0 && (double)qantFER/pass < params.target_FER ) 	break;
		if( itmoFER > 0 && (double)itmoFER/pass < params.target_FER ) 	break;
	}

	QAM_modulator_close( qam_mod_st);

	free( dec_output );
	free( dec_input );
	free( codeword );
	free( dm_buf );
	free( ch_buf[0] );
	free( ch_buf[1] );
	free( QAM_buf[0] );
	free( QAM_buf[1] );

	return 0;
}

#ifdef USE_ANCHOR_DECODER
int QANT_decoder( ldpc_dec_engine_t &dec, int n_iter )
{
	int iter;
		
	for( iter = 0; iter < n_iter; iter++ )
	{
		auto ret = dec.iterate();
		auto parity = dec.calc_parity_check();
//			cout << "iteration output: " << (int)ret << "\n";
//			cout << "final check is : " << (int)parity << "\n";

/*
		if( (int)ret == 2 && parity != false)
			printf("BAD0\n");

		if( (int)ret != 2 && parity == false)
			printf("BAD1\n");
*/
//		if( (int)ret == 2 ) break;
		if( !parity ) break;
	}

	if( iter == n_iter )
		iter = -iter;
	else
		iter += 1;

	return iter;
}
#endif

#ifdef USE_ITMO_DECODER
int ITMO_decoder( itmo_ldpc_dec_engine_t &dec, int n_iter )
{
	int iter;
		
	for( iter = 0; iter < n_iter; iter++ )
	{
		auto ret = dec.iterate();
		auto parity = dec.calc_parity_check();
//			cout << "iteration output: " << (int)ret << "\n";
//			cout << "final check is : " << (int)parity << "\n";
		if( !parity ) break;
	}

	if( iter == n_iter )
		iter = -iter;
	else
		iter += 1;
	return iter;
}
#endif


