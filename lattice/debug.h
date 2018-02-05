#ifndef _DEBUG_H
#define _DEBUG_H

/* level 1 error definition */
#define DBGU_L1_ALGO_INIT		1
#define DBGU_L1_MISMATCH		2
	#define NO_ALGOID			201
	#define COMMENT_END_UNEXPECTED		202
	#define NO_VERSION			203
	#define NO_BUFFERREQ			204
	#define NO_STACKREQ			205
	#define NO_MASKBUFREQ			206
	#define NO_CHANNEL			207
	#define NO_HEADERCS			208
	#define NO_STARTOFALGO			209
	#define NO_COMPRESSION			210
	#define	INIT_SPI_FAIL			211
	#define INIT_ALGO_FAIL			212

#define DBGU_L1_ALGO_PROC		3
	#define UNABLE_TO_GET_BYTE		301
	#define START_PROC_LOOP			302
	#define END_PROC_LOOP			303
	#define NO_NUMBER_OF_REPEAT		304
	#define NO_NUMBER_OF_LOOP		305
	#define NO_NUMBER_OF_WAIT		306
	#define UNRECOGNIZED_OPCODE		307

#define DBGU_L1_PROCESS			4
	#define TRANX_FAIL			401
	#define RUNCLOCK_FAIL			402
	#define REPEAT_FAIL			403
	#define LOOP_FAIL			404
	#define RESETDATA_FAIL			405

#define DBGU_L1_ALGO_TRANX		5
	#define NO_TRANX_OPCODE			501
	#define NO_TRANSOUT_SIZE		502
	#define NO_TRANSOUT_TYPE		503
	#define NO_TRANSOUT_DATA		504
	#define NO_ENDTRAN			505
	#define NO_TRANSIN_SIZE			507
	#define NO_TRANSIN_TYPE			508
	#define NO_TRANSIN_MASK			509
	#define NO_TRANSIN_DATA			510

#define DBGU_L1_TRANX_PROC		6
	#define STARTTRAN_FAIL			601
	#define	TOGGLECS_FAIL			603
	#define TRANX_OUT_ALGO_FAIL		604
	#define TRANX_OUT_PROG_FAIL		605
	#define TRANX_IN_ALGO_FAIL		606
	#define TRANX_IN_PROG_FAIL		607
	#define TRANX_OPCODE_FAIL		608
	#define ENDTRAN_FAIL			609
	#define COMPARE_FAIL			610
	#define DECOMPRESSION_FAIL		612

#define DBGU_L1_REPEAT			7
	#define BUFFER_FAIL			701
	#define STACK_MISMATCH			702
	#define REPEAT_SIZE_EXCEED		703
	#define REPEAT_COND_FAIL		704
	#define REPEAT_COMMENT_FAIL		705

#define DBGU_L1_LOOP			8
	#define LOOP_SIZE_EXCEED		803
	#define LOOP_COND_FAIL			804
	#define LOOP_COMMENT_FAIL		805

/* level 2 debugging definition */
#define	DBGU_L2_INIT			10
	#define INIT_BEGIN			1001
	#define INIT_COMPLETE			1002

#define DBGU_L2_PROC			11
	#define START_PROC			1101
	#define START_PROC_BUFFER		1102
	#define END_PROC			1103
	#define END_PROC_BUFFER			1104
	#define ENTER_STARTTRAN			1105
	#define ENTER_RUNCLOCK			1106
	#define ENTER_REPEAT			1107
	#define ENTER_LOOP			1108
	#define ENTER_WAIT			1109

#define DBGU_L2_REPEAT			12
	#define PREPARE_BUFFER			1201
	#define FINISH_BUFFER			1202
	#define START_PROC_REPEAT		1203
	#define END_PROC_REPEAT			1204

#define DBGU_L2_LOOP			13


/* Error Code */
#define ERROR_INIT_ALGO			-1
#define ERROR_INIT_DATA			-2
#define ERROR_INIT_VERSION		-3
#define ERROR_INIT_CHECKSUM		-4
#define ERROR_INIT_SPI			-5
#define ERROR_INIT			-6
#define ERROR_IDCODE			-7
#define ERROR_USERCODE			-8
#define ERROR_SED			-9
#define ERROR_TAG			-10
#define ERROR_PROC_ALGO			-11
#define ERROR_PROC_DATA			-12
#define ERROR_PROC_HARDWARE		-13
#define ERROR_LOOP_COND			-14
#define ERROR_VERIFICATION		-20

#endif
