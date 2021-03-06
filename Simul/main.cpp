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

#define FAST_DEMODULATOR

//#define USE_ANCHOR_DECODER
#define USE_MY_DECODER
//#define USE_ITMO_DECODER
//#define USE_EXT_DECODER
#define EXT_LIST_LEN  3

//#define SIGMA_DEVIATION 50
//#define SHOW_ITER_HISTOGRAM
//#define SHOW_MATRIX_HISTOGRAM
//#define NORM_LLR_SERGEI_PRE
//#define NORM_LLR_SERGEI_POST
//#define NOSQRT

#ifdef NORM_LLR_SERGEI_POST
#define ADD_QUANT_BITS 1
#endif

#define LLR_LIMIT     31//7
#define QUANT		0.25//1.0
#define QUANT_FPP	2
#define PRE_SHIFT   2//0//-1
#if PRE_SHIFT >= 0 
#define PRE_SHIFT_COEF   (1 << PRE_SHIFT)
#else
#define PRE_SHIFT_COEF   (1.0 / (1 << -(PRE_SHIFT)))
#endif 

#define LLR_BITS	7//6
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

#define DEC_TYPE	 1// 0 - LMS, 1 - ILMS, 2 - LSP, 3 - ILSP   

#define NORM_LLR 0

#define MS_ALPHA 0.8

#define MAX_N_ITER 1000


using namespace std;

int QANT_decoder( ldpc_dec_engine_t &dec, int n_iter );
int ITMO_decoder( itmo_ldpc_dec_engine_t &dec, int n_iter );

#ifdef NORM_LLR_SERGEI_POST
void normllrs(int llrs[], int mode, int n, int pre_shift )
{
	int i, k, qam, r;
	double m2, m4, B, theta, m_est, s, s0, K, log2qam;

	m2 = 0;
	m4 = 0;

	switch (mode)
	{
	case  1: qam = 2; s0 = 0.80;	break;
	case  2: qam = 4; s0 = 0.80;	break;
	case  3: qam = 4; s0 = 0.55;	break;
	case  4: qam = 16; s0 = 0.94;	break;
	case  5: qam = 16; s0 = 0.60;	break;
	case  6: qam = 64; s0 = 0.80;	break;
	case  7: qam = 64; s0 = 0.67;	break;
	case  8: qam = 64; s0 = 0.56;	break;
	case  9: qam = 256; s0 = 0.73;	break;
	case 10: qam = 256;  s0 = 0.6;	break;
	};


	log2qam = log2(qam);
	if (log2qam < 1)
		r = 1;
	else
		r = round(log2qam);

	k = 0;
	for (i = 0; i < n; i++) {
		if (i % r == r - 1)
		{
			m2 = m2 + llrs[i] * llrs[i];
			m4 = m4 + llrs[i] * llrs[i] * llrs[i] * llrs[i];
			k++;
		}
	}

	m2 = m2 / k;
	m4 = m4 / k;
	B = m4 / (m2*m2);

	K = 1;
	if (B < 3)
	{
		theta = (-B + 3 + sqrt(6 - 2 * B)) / (B - 1);
		//m_est = sqrt(m2*theta / (theta + 1));
		//s = sqrt(2 / m_est);
		s = (2 / sqrt(m2))*(sqrt((theta + 1) / theta));
		K = s / s0;
	}

	K *= 1 << pre_shift;
	for (i = 0; i < n; i++)
	{
		theta = llrs[i] * K;
		llrs[i] = round(theta);
	}
}
#endif
#ifdef NORM_LLR_SERGEI_PRE
void normllrs(double llrs[], int mode, int n)
{

	int i, k, qam, r;
	double m2, m4, B, theta, m_est, s, s0, K, log2qam;

	m2 = 0;
	m4 = 0;
	
	double TS[7] = { 0, 2, 6, 12, 80, 200, 10000 };
	double PS[18] = { 0.29636, -1.4625, 3.1592, 0.028134, -0.36456, 2.006, 0.0033268, -0.098624, 1.2846, 0.000081735, -0.011712, 0.65667, 0.0000036604, -0.0016752, 0.33165, 0.00000011956, -0.00022694, 0.17415 };
	double TB[7] = { 1, 2, 2.7, 2.9, 2.95, 2.97, 3.0 };
	double PB[18] = { 0.0799060838,  -0.054681873, 0.976653418, 0.583827925, -2.18416707, 3.23176628, 7.51909625, -39.7135427, 54.0263355, 68.1931215, -391.011457, 562.545787, 270.056961, -1581.60853, 2318.08933, 1467.44101, -8700.7903, 12900.0542 };
	
	double P[3], PBB[3], sqrtm2, f;

	switch (mode)
	{
	case  1: qam = 2; s0 = 0.80;	break;
	case  2: qam = 4; s0 = 0.80;	break;
	case  3: qam = 4; s0 = 0.55;	break;
	case  4: qam = 16; s0 = 0.94;	break;
	case  5: qam = 16; s0 = 0.60;	break;
	case  6: qam = 64; s0 = 0.80;	break;
	case  7: qam = 64; s0 = 0.67;	break;
	case  8: qam = 64; s0 = 0.56;	break;
	case  9: qam = 256; s0 = 0.73;	break;
	case 10: qam = 256;  s0 = 0.6;	break;
	};


	log2qam = log2(qam);
	if (log2qam < 1)
		r = 1;
	else
		r = (int)round(log2qam);

	k = 0;
	for (i = 0; i < n; i++) {
		if (i % r == r - 1)
		{
			m2 = m2 + llrs[i] * llrs[i];
			m4 = m4 + llrs[i] * llrs[i] * llrs[i] * llrs[i];
			k++;
		}
	}

	m2 = m2 / k;
	m4 = m4 / k;
	B = m4 / (m2*m2);
	K = 1.0;

#ifdef NOSQRT
	int j = 0;
	if (B < 3) {
		while (m2 > TS[j - 1 + 1]) {
			for (i = 0; i < 3; i++)
				P[i] = PS[i + j * 3];
			j++;
			if (j > 6)
				break;
		}

		sqrtm2 = m2 * m2*P[0] + m2 * P[1] + P[2];

		j = 0;
		for (i = 0; i < 3; i++)
			P[i] = 0;

		while (B > TB[j - 1 + 1]) {
			for (i = 0; i < 3; i++)
				P[i] = PB[i + j * 3];
			j++;
			if (j > 6)
				break;
		}
		f = B * B*P[0] + B * P[1] + P[2];
		s = sqrtm2 * f;

		K = s / s0;
	}

	for (i = 0; i < n; i++)
	{
		llrs[i] = llrs[i] * K;
	}

#else
	if (B < 3)
	{
		theta = (-B + 3 + sqrt(6 - 2 * B)) / (B - 1);
		//m_est = sqrt(m2*theta / (theta + 1));
		//s = sqrt(2 / m_est);
		s = (2 / sqrt(m2))*(sqrt((theta + 1) / theta));
		K = s / s0;
	}

	for (i = 0; i < n; i++)
	{
		llrs[i] = llrs[i] * K;
	}
#endif
}

#endif


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

static int** Alloc2d_int( int b, int c )
{
	int **p;
	int i;

//	p = (int**)calloc( b, sizeof(int*) );
	p = (int**)malloc( b * sizeof(int*) );
	//assert(p);
//	p[0] = (int*)calloc( b*c, sizeof(int) );
	p[0] = (int*)malloc( b*c*sizeof(int) );
	for( i = 1; i < b; i++ )
	{
		p[i] = p[i-1] + c;
		//assert( p[i] );
	}
	return p;
}


static void free2d_int( int **p )
{
	free( p[0] );
	free( p );
}


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
	int itmo_extBER;
	int itmo_extFER;
	int *matr_ptr;
	DEC_STATE *dec_state = NULL;
	QAM_MODULATOR_STATE* qam_mod_st = NULL;	
	QAM_DEMODULATOR_STATE* qam_demod_st = NULL;	

	int il_inner_data_bits;
	CODE_CFG code;
	SNR_CFG snr_cfg;
	DEC_RES result;
	int *codeword;
	int *syndrome;
	int *dec_input;
	int **hd_matrix;
	int *dec_output;
	double *ch_buf[2];
	double *ch_buffer;
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
	int max_llr;	
	double quant, inv_quant;
	int pre_shift;
	int iter_hist[MAX_N_ITER+2];
	int matr_hist[MAX_STATE];
	double all_QANT_FER[N_SNR];
	double all_ITMO_FER[N_SNR];
	double all_SNR[N_SNR];
	char *fileName;
	char sdtFileName[] = "result.txt";
	SIMULATION_PARAMS params;
	FILE *fp;

	//default parameters
	fileName = sdtFileName;
	params.cfg = CFG;
	params.code_M = CODE_M;
	params.n_pass = N_PASS;
	params.n_iter = N_ITER;
	params.n_event = N_EVENT;
	params.llr_bits = LLR_BITS;
	params.dec_type = DEC_TYPE;
	params.target_FER = TARGET_FER;
	params.SNR_flag = 0;
	params.llr_limit = LLR_LIMIT;
	params.file_name[0] = '\0';

	if( argc == 2 )
	{
		params.cfg = 0;
		fileName = argv[1];
		set_params( fileName, &params );
	}

	norm_llr = NORM_LLR;

	il_inner_data_bits = params.llr_bits + IL_SOFT_FPP;

#ifdef NO_MODULATION 
	switch( cfg )
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
	case  0: QAM =   1; irate = 0; snr_cfg = { 1.0,  3.0, 0.25 };	break;	// no modulation; 
	case  1: QAM =   2; irate = 1; snr_cfg = { 1.0,  2.0, 0.25 };	break;	// BPSK; R=1/2
	case  2: QAM =   4; irate = 1; snr_cfg = { 1.0,  2.0, 0.25 };	break;	// QPSK; R=1/2
	case  3: QAM =   4; irate = 3; snr_cfg = { 2.0,  3.00, 0.25 };	break;	// QPSK; R=3/4
	case  4: QAM =  16; irate = 1; snr_cfg = { 3.0,  4.5, 0.25 };	break;	//       R=1/2
	case  5: QAM =  16; irate = 3; snr_cfg = { 4.5,  6.25, 0.25 };	break;	//       R=3/4
	case  6: QAM =  64; irate = 2; snr_cfg = { 7.5,  8.75, 0.25 };	break;	//       R=2/3
	case  7: QAM =  64; irate = 3; snr_cfg = { 8.5, 9.75, 0.25 };	break;	//       R=3/4
	case  8: QAM =  64; irate = 4; snr_cfg = { 9.5, 11.00, 0.25 };	break;	//       R=5/6
	case  9: QAM = 256; irate = 3; snr_cfg = {12.5, 13.75, 0.25 };	break;	//       R=3/4
	case 10: QAM = 256; irate = 4; snr_cfg = {13.5, 15.25, 0.25 };	break;	//       R=5/6

	default: QAM =   2, irate =1;  snr_cfg = { 1.0,  3.0, 0.25 };	break;	// BPSK; R=1/2
	};

	if( irate )
		code = select_code( irate, params.code_M );
	else
	{
		code.M =  params.code_M;
		code.matrix = NULL;
		code.ncol = 0;
		code.nrow = 0;
	}
#endif


	if( params.SNR_flag == 0 )
	{
		params.SNR_start = snr_cfg.start_snr;
		params.SNR_stop  = snr_cfg.stop_snr;
		params.SNR_step  = snr_cfg.step_snr;
	}

	switch( QAM )
	{
	case 1:		bps = 1; halfmlog = 1;	break;
	case 2:		bps = 1; halfmlog = 1;	break;
	case 4:		bps = 2; halfmlog = 1;	break;
	case 16:	bps = 4; halfmlog = 2;	break;
	case 64:	bps = 6; halfmlog = 3;	break;
	case 256:	bps = 8; halfmlog = 4;	break;
	};

	vector< vector<int> > check_matrix(code.nrow, vector<int>(code.ncol, 0));

	if( code.matrix )
	{
		// code is selected successefully
		// initialize check matrix
		matr_ptr = code.matrix;
		for( i = 0; i < code.nrow; i++ )
			for( j = 0; j < code.ncol; j++ )
				check_matrix[i][j] = *matr_ptr++;
	}


#ifdef USE_EXT_DECODER
	if( params.dec_type < 0 )
		code = open_ext_il_minsum( params.file_name, code.M );
	else
		open_ext_il_minsum( irate, code.M, EXT_LIST_LEN );
#endif

	hd_matrix = Alloc2d_int( code.nrow, code.ncol );
	matr_ptr = code.matrix;
	for( i = 0; i < code.nrow; i++ )
		for( j = 0; j < code.ncol; j++ )
			hd_matrix[i][j] = *matr_ptr++;

	codelen = code.M * code.ncol;
	nQAMSignals = codelen / bps;

	vector<int> llr(codelen);

	if( params.dec_type >= 0 )
	{
		if( dec_state )
			decod_close( dec_state );

		dec_state = decod_open( params.dec_type, code.nrow, code.ncol, code.M );
		if (!dec_state)
		{
			printf("Can't open decode\n");
			return 0;
		}

		if( code.matrix )
		{
			// code is selected successefully
			// initialize check matrix
			for( i = 0; i < code.nrow; i++ )
				for( j = 0; j < code.ncol; j++ )
					dec_state->hd[i][j] = hd_matrix[i][j];
		}
	}


	bitrate = (double)(code.ncol - code.nrow) / code.ncol;
	inflen  = (code.ncol - code.nrow) * code.M;

	dec_input  = (int*)malloc( codelen * sizeof(dec_input[0]) );
	dec_output = (int*)malloc( codelen * sizeof(dec_output[0]) );
	codeword   = (int*)malloc( codelen * sizeof(codeword[0]) );
	syndrome   = (int*)malloc( codelen * sizeof(syndrome[0]) );
	QAM_buf[0] = (int*)malloc( codelen/bps * sizeof(QAM_buf[0][0]) );
	QAM_buf[1] = (int*)malloc( codelen/bps * sizeof(QAM_buf[0][0]) );
	ch_buf[0]  = (double*)malloc( codelen/bps * sizeof(ch_buf[0][0]) );
	ch_buf[1]  = (double*)malloc( codelen/bps * sizeof(ch_buf[0][0]) );
	dm_buf     = (double*)malloc( codelen * sizeof(dm_buf[0]) );

#ifdef FAST_DEMODULATOR
	ch_buffer  = (double*)malloc( codelen/bps*2 * sizeof(ch_buffer[0]) );
#endif

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
	//codeword[0] = 1;
#endif

	encode_qc( codeword, hd_matrix, code.M, code.nrow, code.ncol, syndrome );

	// initialize QANT decoder
#ifdef USE_ANCHOR_DECODER
	dec_engine.init(check_matrix, code.M);
#endif

#ifdef USE_ITMO_DECODER
	itmo_dec_engine.init(check_matrix, code.M);
#endif

	if( QAM > 2 )
		QAM_modulator( qam_mod_st, codeword, QAM_buf  );


#ifdef ADD_QUANT_BITS
	max_llr = (1 << (params.llr_bits-1+ADD_QUANT_BITS)) - 1;
#else
	max_llr = (1 << (params.llr_bits-1)) - 1;
#endif


	quant = (double) (params.llr_limit+1) / (max_llr + 1);

	inv_quant = 1.0 / quant;

	if( max_llr > params.llr_limit )
	{
		int tmp = max_llr;
		pre_shift = 0;
		while( tmp > params.llr_limit )
		{
			tmp >>= 1;
			pre_shift += 1;
		}
	}
	else
	{
		int tmp = params.llr_limit;
		pre_shift = 0;
		while( tmp > max_llr )
		{
			tmp >>= 1;
			pre_shift -= 1;
		}

	}

#ifdef ADD_QUANT_BITS
		pre_shift -= ADD_QUANT_BITS;
#endif

 	snr_ind = 0;
	for( snr = params.SNR_start; snr <= params.SNR_stop; snr += params.SNR_step )
	{
		printf("\n=======================\n");
		//sigma = sqrt(pow(10, -snr / 10) / 2 / bitrate);

		norm_factor = QAM > 2 ? 2*(QAM-1)/3 : 1;

		sigma = sqrt( pow(10, -snr / 10) / (2 * bitrate *bps) * norm_factor );

		printf("Eb/No = %f, sigma = %f\n", snr, sigma );
#ifdef SIGMA_DEVIATION
		printf("SIGMA DEVIATION= [%d %d] percent\n", -SIGMA_DEVIATION, SIGMA_DEVIATION );
#endif

		qantBER = 0;
		qantFER = 0;
		itmoBER = 0;
		itmoFER = 0;
		itmo_extBER = 0;
		itmo_extFER = 0;
		
		memset( matr_hist, 0, sizeof(matr_hist) );
		memset( iter_hist, 0, sizeof(iter_hist) );

		if( qam_demod_st )
			QAM_demodulator_close(qam_demod_st );
		qam_demod_st = QAM_demodulator_open( T, QAM, codelen, bps, nQAMSignals, 0 );
		if( !qam_demod_st )
		{
			printf("Can't open QAM demodulator");
			return 0;
		}

		for( pass = 0; pass < params.n_pass; pass++ )
		{
			double e_sigma;	// estimated sigma; 

			e_sigma = sigma;

#ifdef SIGMA_DEVIATION 
			double dev = rand() % (SIGMA_DEVIATION*2+1) - SIGMA_DEVIATION;
			dev = 1.0 + 0.01 * dev;
			e_sigma *= dev;
#endif

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

#ifndef FAST_DEMODULATOR
				memset( dm_buf, 0, codelen * sizeof(dm_buf[0] ) );
				Demodulate( qam_demod_st, ch_buf, dm_buf, e_sigma );
				for( i = 0; i < codelen; i++ )	dm_buf[i] = -dm_buf[i];
#else
				for( i = 0; i < codelen/bps; i++ )
				{
					ch_buffer[i*2+0] = ch_buf[0][i];
					ch_buffer[i*2+1] = ch_buf[1][i];
				}

				PAM_Demodulate( qam_demod_st, ch_buffer, dm_buf, e_sigma );
#endif
			}
			else
			{
				for( i = 0; i < codelen; i++ )
				{
					double noise = next_random_gaussian();

					dm_buf[i] = -2.0 * (sigma * noise + (2.0 * codeword[i] - 1.0) ) / (e_sigma * e_sigma);	
				}
			}

			if( norm_llr )
				normalize( dm_buf, codelen, max_llr );
			
#ifdef NORM_LLR_SERGEI_PRE
			normllrs(dm_buf, params.cfg, codelen);
#endif
			

			// llrs quantization
			for( i = 0; i < codelen; i++ )
			{
				double llr_limit = params.llr_limit;
				double real_llr = dm_buf[i];
				double abs_llr  = (real_llr < 0.0 ? -real_llr : real_llr);
				double lim_llr  = abs_llr < llr_limit ? abs_llr : llr_limit;
				int     int_llr = (int)(lim_llr * inv_quant + 0.5);

				int_llr = int_llr <  max_llr ? int_llr :  max_llr;
				dec_input[i] = llr[i] = ((real_llr < 0.0 ? 1 : 0) ? -int_llr : int_llr);
			}


#ifdef NORM_LLR_SERGEI_POST
#ifdef ADD_QUANT_BITS
			normllrs( dec_input, params.cfg, codelen, pre_shift+ADD_QUANT_BITS );
#else
			normllrs( dec_input, params.cfg, codelen, pre_shift );
#endif
#endif


#ifdef ADD_QUANT_BITS
			for( i = 0; i < codelen; i++ )
			{
				int sign = dec_input[i] < 0 ? 1 : 0;
				int val = dec_input[i] < 0 ? -dec_input[i] : dec_input[i];
				val = (val + (1 << (ADD_QUANT_BITS-1))) >> ADD_QUANT_BITS;
				dec_input[i] = sign ? -val : val; 
			}
#endif

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

			qantBER += nerr;
			qantFER += qant_dec_OK == 0;

#endif
#ifdef USE_ITMO_DECODER
			itmo_dec_engine.reset();
			itmo_dec_engine.push(llr);
			int itmo_iter = ITMO_decoder( itmo_dec_engine, params.n_iter );
			auto itmo_dec_cw = itmo_dec_engine.pull();

			for( i = 0, nerr = 0; i < codelen; i++ )
			{
				if( codeword[i] != (int)itmo_dec_cw[i] )
					nerr++;
			}
			itmo_dec_OK = nerr == 0 ? 1 : 0;

			itmoBER += nerr;
			itmoFER += itmo_dec_OK == 0;

			if( itmo_dec_OK == 0 && itmo_iter > 0 )
				i = i;
#endif

#ifdef USE_EXT_DECODER
			if( pass == 8 )
				pass = pass;

			result = ext_il_min_sum( dec_input, dec_output, params.n_iter, MS_ALPHA, 0.5, il_inner_data_bits );
			itmo_iter = result.iter;

			for( i = 0, nerr = 0; i < codelen; i++ )
			{
				if( codeword[i] != (int)dec_output[i] )
					nerr++;
			}
			itmo_dec_OK = nerr == 0 ? 1 : 0;
			
//			itmo_extBER += nerr;
//			itmo_extFER += itmo_dec_OK == 0;
			itmoBER += nerr;
			itmoFER += itmo_dec_OK == 0;

			if( itmo_dec_OK )
				matr_hist[result.dec_index] += 1;

#endif
			
		//	printf( "QANT iterations: %5d, result: %s\n", iter < params.n_iter ? iter + 1 : iter, dec_OK ? "OK" : "ERROR" );


		//	for( i = 0; i < codelen; i++ )	dec_output[i] = 0;

#ifdef USE_MY_DECODER
			switch( params.dec_type )
			{
			case LMS_DEC:   itmo_iter = lmin_sum_decod_qc_lm( dec_state, dec_input, dec_output, params.n_iter, MS_ALPHA, 0.5, pre_shift ); break;
			case ILMS_DEC:  itmo_iter = il_min_sum_decod_qc_lm( dec_state, dec_input, dec_output, params.n_iter, MS_ALPHA, 0.5, il_inner_data_bits, pre_shift ); break;
			case LCHE_DEC:  itmo_iter = lche_decod( dec_state, dec_input, dec_output, params.n_iter );		 break;
			case ILCHE_DEC: itmo_iter = ilche_decod( dec_state, dec_input, dec_output, params.n_iter );		 break;
			default: itmo_iter = -1; printf("Unknown decoder type: %d\n", params.dec_type); return 0;
			}

			//if( itmo_iter != itmo_iter1 )
			//{
			//	printf("!!!!!!!\n");
			//}

	  		for( i = 0, nerr = 0; i < codelen; i++ )
			{
				if( codeword[i] != dec_output[i] )
					nerr++;
			}
			itmo_dec_OK = nerr == 0 ? 1 : 0;


			itmoBER += nerr;
			itmoFER += itmo_dec_OK == 0;

			if( itmo_dec_OK == 0 && itmo_iter > 0 )
				i = i;

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

			if( itmo_iter > MAX_N_ITER )	itmo_iter = MAX_N_ITER;
			if( itmo_iter < 0 )				itmo_iter = params.n_iter + 1;
			iter_hist[itmo_iter] += 1;

			if( itmoFER == params.n_event ){	pass += 1;	break;	}
#if !defined( USE_MY_DECODER ) && !defined( USE_ITMO_DECODER )
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
#ifdef SHOW_ITER_HISTOGRAM	
		printf("iteration histogram: \n");
		for( int i = 0; i < params.n_iter+2; i++ )
			printf( "%4d ", iter_hist[i] );
		printf( "\n" );
#endif
#ifdef SHOW_MATRIX_HISTOGRAM	
		printf("matrix histogram: \n");
		for( int i = 0; i < 8; i++ )
		{
			if( (i % 4) == 0 && i) printf("\n");
			printf( "%6d ", matr_hist[i] );
		}
		printf( "\n" );
#endif


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
#ifdef SHOW_ITER_HISTOGRAM
			fprintf( fp, "iteration histogram: ");
			for( int i = 0; i < params.n_iter + 2; i++ )
				fprintf( fp, "%4d ", iter_hist[i] );
			fprintf( fp, "\n" );
#endif
#ifdef SHOW_MATRIX_HISTOGRAM	
			fprintf( fp, "matrix histogram: \n");
			for( int i = 0; i < 8; i++ )
			{
				if( (i % 4) == 0 && i) printf("\n");
				fprintf( fp, "%4d ", matr_hist[i] );
			}
			fprintf( fp, "\n" );
#endif

			fclose( fp );
		}

		all_QANT_FER[snr_ind] = (double)qantFER / pass;
		all_ITMO_FER[snr_ind] = (double)itmoFER / pass;
		all_SNR[snr_ind]      = snr;

		snr_ind++;


		if( qantFER > 0 && (double)qantFER/pass < params.target_FER ) 	break;
		if( itmoFER > 0 && (double)itmoFER/pass < params.target_FER ) 	break;
	}

	fopen_s( &fp, fileName, "at" );
	if( fp == NULL )
	{
		printf("file error\n");
		fclose(fp);
	}
	else
	{
		fprintf( fp, "\n" );
		fprintf( fp, "SNR:       " );
		for( int i = 0; i < snr_ind; i++ )
			fprintf( fp, "%8.6f ", all_SNR[i] );
		fprintf( fp, "\n" );

#ifdef USE_ANCHOR_DECODER
		fprintf( fp, "QANT_FERR: " );
		for( int i = 0; i < snr_ind; i++ )
			fprintf( fp, "%8.6f ", all_QANT_FER[i] );
		fprintf( fp, "\n" );
#endif

		fprintf( fp, "ITMO_FERR: " );
		for( int i = 0; i < snr_ind; i++ )
			fprintf( fp, "%8.6f ", all_ITMO_FER[i] );
		fprintf( fp, "\n" );
	}	
	fclose( fp );

	QAM_modulator_close( qam_mod_st);


	if( dec_state )
		decod_close( dec_state );

#ifdef USE_EXT_DECODER
	close_ext_il_minsum();
#endif


	free( dec_output );
	free( dec_input );
	free( codeword );
	free( syndrome );
	free( dm_buf );
	free( ch_buf[0] );
	free( ch_buf[1] );
#ifdef FAST_DEMODULATOR
	free( ch_buffer );
#endif
	free( QAM_buf[0] );
	free( QAM_buf[1] );
	free2d_int( hd_matrix );

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

		if( ret == ldpc_dec_engine_t::ret_status::ET ) 
			break;

//		if( !parity ) 
//			break;
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
		if( ret == itmo_ldpc_dec_engine_t::ret_status::ERROR )
			break;	// engine is not initialized
		if( ret == itmo_ldpc_dec_engine_t::ret_status::ET ) 
			break;
//		if( !parity ) 
//			break;
	}

	if( iter == n_iter )
		iter = -iter;
	else
		iter += 1;
	return iter;
}
#endif


