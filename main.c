#include <windows.h>
#include <stdio.h>
#include <time.h>

//program parameters
//file names
#define FILE1_DEFAULT "file1.txt"
#define FILE2_DEFAULT "file2.txt"
char *fname_i1;		//param -if1=... - input 1 file name - master list (default: file2.txt)
char *fname_i2;		//param -if2=... - input 2 (default: file2.txt)
char *fname_o;		//param -of=... - output (default: out.txt)
//search string positions
int f1_data_idx;	//param -f1i=... (default: 0)
int f1_data_len;	//param -f1l=... (default: 0)
int f2_data_idx;	//param -f2i=... (default: 0)
int f2_data_len;	//param -f2l=... (default: 0)
//switches:
int verbose = 0;	//print extra stuff to the screen, if output is file (default: 0-no)
int multiple_lines = 0;	//list all matching lines from file2, not just the first default: 0-no)
int case_insensitive = 0;	//param -c - (default: 0-no) - NOT WORKING YET

char** file2data;
size_t file2data_len = 0;

FILE *fp_input1;
FILE *fp_input2;
FILE *fp_output;

//------------------------------------------------------------------------------------------------------------------
void print_usage(void)
{
	printf("=== tool for merging two text files ===\n");
	printf("usage prog.exe [-if1=file1.txt] [-if2=file2.txt] [-of=outfile.txt] [-f1i=n] [-f1l=n] [-f2i=n] [-f2l=n] [-v] [-m]\n");
	printf("*** WARNING: otput file will be overwritten during run!\n");
	printf("params: [-if1: input file #1](def: file1.txt) [-if2: input file #2](def: file2.txt) [-of: output file](def: stdout)\n");
	printf("params for file #1: [-f1i: substring index] [-f1l: substring length] (def: whole line)\n");
	printf("params for file #2: [-f2i: substring index] [-f2l: substring length] (def: whole line)\n");
	printf("switches (can't be concatenated!): [-v: verbose](def: no) [-m: get multiple lines from file2] (def: no) \n");
}

//------------------------------------------------------------------------------------------------------------------

char* get_param_str(char* full_param)
{
	char* p = strchr(full_param, '=');
	if (!p){
		return NULL;
	}
	return p + 1;
}
//------------------------------------------------------------------------------------------------------------------
int check_indexes(int idx, int val_start, int val_end)
{
	if (val_start < 0 || val_end < 0){
		printf("*** Some of i%d values < 0\n", idx);
		return -1;
	}
	return 0;
}
//------------------------------------------------------------------------------------------------------------------
void print_params(void)
{
	printf("CURRENT PARAMS:\n");
	printf("fname_i1=%s\n", fname_i1);
	printf("fname_i2=%s\n", fname_i2);
	printf("fname_o=%s\n", fname_o);
	printf("f1_data_idx=%d\n", f1_data_idx);
	printf("f1_data_len=%d\n", f1_data_len);
	printf("f2_data_idx=%d\n", f2_data_idx);
	printf("f2_data_len=%d\n", f2_data_len);
	printf("multiple_lines=%d\n", multiple_lines);
	printf("verbose=%d\n", verbose);
	printf("case_insensitive=%d (NOT IMPLEMENTED YET)\n", case_insensitive);
	printf("\n");
}

//------------------------------------------------------------------------------------------------------------------
//returns new len
int remove_line_endings(char* input)
{
	int ilen = strlen(input);

	//remove '\n' or '\r' at the end
	while(ilen && (input[ilen - 1] == '\n' || input[ilen - 1] == '\r')){
		input[ilen - 1] = 0;
		ilen--;
	}
	return ilen;
}

//------------------------------------------------------------------------------------------------------------------
int get_chunk_len(char* input, int idx_start, int len)
{
	int ilen = remove_line_endings(input);

	if(idx_start >= ilen){
		return 0;
	}
	else if(idx_start + len > ilen){
		return 0;
	}
	else if(len == 0){
		if(idx_start == 0){
			len = ilen;
		}
		else{
			len = ilen - idx_start;	//???
		}
	}
	return len;

}

//------------------------------------------------------------------------------------------------------------------
void free_i2(void)
{
	for (int i = file2data_len - 1; i >= 0; i--)
	{
		if(file2data[i] != NULL){
			free(file2data[i]);
		}
	}
}

//------------------------------------------------------------------------------------------------------------------
void print_line(char* line1, char* line2, int ditto)
{
	const char* ditto_str = "-||-";
	const int ditto_len = 4;
	int len1 = strlen(line1);
	int len;

	if (ditto){
		len = len1 > ditto_len ? len1 - ditto_len : 0;
		fprintf(fp_output, "%s%*s | %s\n", ditto_str, len, "", line2);
	}
	else{
		len = len1 >= ditto_len ? 0 : ditto_len - len1;
		fprintf(fp_output, "%s%*s | %s\n", line1, len, "", line2);
	}
	
}

//------------------------------------------------------------------------------------------------------------------

int file_open(char *fname_i, char *open_mode_i, FILE **fhandler_o, char *errmsg_io, int errmsg_maxlen)
//returns err from fopen_s
//clears errmsg_io
{
	errno_t err;
	char errmsgbuf[100] = {0};
	int rv = 0;

	memset(errmsg_io, 0x00,  errmsg_maxlen);
	err = fopen_s(fhandler_o, fname_i, open_mode_i);
	if (err) {
		if (strerror_s(errmsgbuf, sizeof(errmsgbuf), err)) {
			strcpy_s(errmsgbuf, sizeof(errmsgbuf), "unknown error (default)");
		}
		strcpy_s(errmsg_io, errmsg_maxlen -1, errmsgbuf);
		rv = err;
	}
	return rv;
}

//------------------------------------------------------------------------------------------------------------------
//convert seconds as format HH:MM:SS
char *get_printable_seconds(int seconds)
{
	static char ret_str[] = "hh:mm:ss";
	int hours, minutes;
	hours = seconds / 3600;
	seconds %= 3600;
	minutes = seconds / 60;
	seconds %= 60;

	if(hours <= 99){
		sprintf(ret_str, "%02d:%02d:%02d", hours, minutes, seconds);
	}
	return ret_str;
}

//------------------------------------------------------------------------------------------------------------------

/*convert usedmem to human readable format*/
/*input: 1024, output: "1 Kbyte"*/
char *get_printable_memory(unsigned long int mem)
{
	char xbytes = ' ';
	const char bytesteps[] = " KMGT";
	const char byte_literal[] = "byte";
	int i;
	static char ret_str[100];
	unsigned long int printmem = 0;

	for (i = 0; i < sizeof(bytesteps) - 1; i++) {
		printmem = mem >> (i * 10);
		xbytes = bytesteps[i];
		/*printf("printmem: %lu, xbytes: %c\n", printmem, xbytes);*/
		if (printmem < 1024)
			break;
	}
	sprintf(ret_str, "%lu %c%s%c", printmem, xbytes, byte_literal, (printmem > 1) ? 's' : '\0');
	return ret_str;
}

//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------

int main(int argc, char *argv[])
{
#define MAXLINE 10000
	char input_line[MAXLINE + 1] = { 0 };
	long fsize1;
	int err = 0;
	
	if (argc > 1) {
	//parse params
		int i;
		for (i = 1; i < argc; i++) {
			if (strstr(argv[i], "-c")) {
				case_insensitive = 1;
			}
			else if (strstr(argv[i], "-v")) {
				verbose = 1;
			}
			else if (strstr(argv[i], "-m")) {
				multiple_lines = 1;
			}
			else if (strstr(argv[i], "-if1=")) {
				fname_i1 = get_param_str(argv[i]);
			}
			else if (strstr(argv[i], "-if2=")) {
				fname_i2 = get_param_str(argv[i]);
			}
			else if (strstr(argv[i], "-of=")) {
				fname_o = get_param_str(argv[i]);
			}
			else if (strstr(argv[i], "-f1i")) {
				f1_data_idx = atoi(get_param_str(argv[i]));	//seems atoi works on NULL
			}
			else if (strstr(argv[i], "-f1l")) {
				f1_data_len = atoi(get_param_str(argv[i]));
			}
			else if (strstr(argv[i], "-f2i")) {
				f2_data_idx = atoi(get_param_str(argv[i]));
			}
			else if (strstr(argv[i], "-f2l")) {
				f2_data_len = atoi(get_param_str(argv[i]));
			}
			else{
				//invalid param
				printf("***invalid param: \"%s\"\n", argv[i]);
				err = 1;
			}
		}
	}

	//testing
	/*
	if (verbose){
		printf("--- VERBOSE - print current params: ---\n");
		print_params();
	}
	*/

	//check index values
	if (check_indexes(1, f1_data_idx, f1_data_len) || check_indexes(2, f2_data_idx, f2_data_len)){
		return -1;
	}

	//open input and output files
	{
		char errmsgbuf[100];
		if (fname_i1 == NULL) {
			fname_i1 = FILE1_DEFAULT;
		}
		if (file_open(fname_i1, "rb", &fp_input1, errmsgbuf, sizeof(errmsgbuf))) {
			printf("*** cannot open input file #1 '%s': %s\n", fname_i1, errmsgbuf);
			err = 1;
		}

		if (fname_i2 == NULL) {
			fname_i2 = FILE2_DEFAULT;
		}
		if (file_open(fname_i2, "rb", &fp_input2, errmsgbuf, sizeof(errmsgbuf))) {
			printf("*** cannot open input file #2 '%s': %s\n", fname_i2, errmsgbuf);
			err = 1;
		}

		if (fname_o != NULL) {
			if (file_open(fname_o, "w", &fp_output, errmsgbuf, sizeof(errmsgbuf))) {
				printf("*** cannot open output file '%s': %s\n", fname_o, errmsgbuf);
				err = 1;
			}
		}
		else {
			fp_output = stdout;
		}


	}

	//return if problem
	if (err){
		printf("***param error\n");
		print_params();
		print_usage();
		return -1;
	}
	else if(verbose){
		print_params();
	}


	//get file1 size
	fseek(fp_input1, 0L, SEEK_END);
	fsize1 = ftell(fp_input1);
	fseek(fp_input1, 0L, SEEK_SET);

	//count lines in file2 (file2data_len)
	{
		char ch;
		rewind(fp_input2);
		file2data_len = 0;
		while(!feof(fp_input2))
		{
			ch = fgetc(fp_input2);
			if(ch == '\n')
			{
				file2data_len++;
			}
		}
	}

	//alloc memory for file2data
	{
		size_t malloc_size = sizeof (char*) * file2data_len;
		file2data = malloc(malloc_size);
		if (file2data == NULL){
			printf("***malloc of %d bytes failed!\n", malloc_size);
			return -2;
		}
		else if(verbose){
			printf("%s allocated...\n", get_printable_memory(malloc_size));
		}
		memset(file2data, 0x00, malloc_size);
	}

	//fill file2data
	rewind(fp_input2);
	{
		char input_line[MAXLINE + 1] = { 0 };
		char* p;
		int c = 0;
		long filepos = 0;
		int len;
		long allocated_mem = 0;
		while (!feof(fp_input2)) {
			p = fgets(input_line, sizeof(input_line), fp_input2);
			if(p == NULL){
				//printf("*** read aborted!\n");
				break;
			}
			len = remove_line_endings(input_line);
			//we don't check if (len == 0) here
			p = malloc(len + 1);
			if (!p){
				printf("*** Malloc problem (input2 line #%d):\n%s\n", file2data_len, input_line);
				return -2;
			}
			allocated_mem += len + 1;
			memcpy(p, input_line, len);
			p[len] = '\0';
			file2data[c++] = p;
		}
		if(verbose){
			printf("%s memory allocated for file2 data\n", get_printable_memory(allocated_mem));
		}
	}
	//test file2data
	/*
	printf("--------------- test file2data\n");
	for (size_t i = 0; i < file2data_len; i++)
	{
		printf("%s\n", file2data[i]);
	}
	*/

	//go through file1
	{
		int err_malloc;
		char* p2;
		char* p1;
		//int c = 0;
		int len1, len2;
		int found = 0;
		char tmp_str1[MAXLINE + 1];
		char tmp_str2[MAXLINE + 1];
		clock_t timer1;
		clock_t wait_interval_ticks = 5 * CLOCKS_PER_SEC;
		srand(time(NULL));

		clock_t timer_update_ticks = clock();
		clock_t timer_start_ticks = timer_update_ticks;
		while (!feof(fp_input1)) {
			p1 = fgets(input_line, sizeof(input_line), fp_input1);
			if(p1 == NULL){
				//printf("*** read aborted!\n");
				break;
			}
			len1 = get_chunk_len(input_line, f1_data_idx, f1_data_len);
			memcpy(tmp_str1, p1 + f1_data_idx, len1);

			found = 0;
			int found_prev = 0;
			for (size_t i = 0; i < file2data_len; i++){
				found_prev = found;
				p2 = file2data[i];
				len2 = get_chunk_len(p2, f2_data_idx, f2_data_len);
				memcpy(tmp_str2, p2 + f2_data_idx, len2);
				tmp_str2[len2] = '\0';
				if(len1 == len2){
					if(memcmp(tmp_str1, tmp_str2, len1) == 0){
						found++;
					}
				}
				else if(len1 > len2){
					if (strstr(tmp_str1, tmp_str2) != NULL){
						found++;
					}
				}
				else{
					if (strstr(tmp_str2, tmp_str1) != NULL){
						found++;
					}
				}

				if (found > found_prev){
					print_line(input_line, p2, found > 1);
					if (found > 0 && !multiple_lines){
						break;
					}
				}
			}
			if(!found){
				print_line(input_line, "***NOT FOUND", 0);
			}

			//progress displayed here
			if(verbose && fp_output != stdout){
				if (clock() - timer_update_ticks >= wait_interval_ticks){
					float curr_pos = (float)ftell(fp_input1);
					float curr_pos_out = (float)ftell(fp_output);
					float percent = curr_pos * 100 / fsize1;
					int estimated_out_size_bytes = (int)((100.0 / percent) * curr_pos_out);
					float seconds_so_far = (float)(clock() - timer_start_ticks) / CLOCKS_PER_SEC;
					float seconds_per_percent = seconds_so_far / percent;
					int estimated_seconds_left = (100.0 - percent) * seconds_per_percent;
					printf("Progress: %03d%% (estimations -> output file size: %s, time left: %s)\n",
						(int)percent, 
						get_printable_memory(estimated_out_size_bytes), 
						get_printable_seconds(estimated_seconds_left)
					);
					timer_update_ticks = clock();
				}
			}
		}
	}
	

	//closing files
	fclose(fp_input1);
	fclose(fp_input2);
	if(fp_output != stdout){
		fclose(fp_output);
	}

	//free_records();
	free_i2();
	if(file2data){
		free(file2data);
	}
	return 0;
}

//------------------------------------------------------------------------------------------------------------------

