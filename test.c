#include "mm.h"
#include "memlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define ARSZ 200 
#define BLKSZ 50

int test_init()
{
 mem_init();
 if (!(mm_init())) return 0;
 else return 1; 
}

void exit_status()
{
 print_free_list();
 print_blocks();
}

int test_single(int );
int free_single(int);
int get_user();
int process(int arg);
void options();
int test_block(int);

int *tmp[ARSZ];
int last_used = -1;

int main()
{
 int t=0;
 srand(time(0));
 // initializiation testing 
 if (!test_init()) printf("\nInitialized.\n");
 else{
  printf("\nInitialization failed.\n");
  return EXIT_FAILURE;
 }

 options();
 while ( (t=get_user()) ){
  process(t);
 } 

//  exit_status();
  return EXIT_SUCCESS;
}

void options(){
 printf("\t0\tExit\n");
 printf("\t1\tHelp\n");
 printf("\t2\tStatus\n");
 printf("\t3\tmalloc single integer\n");
 printf("\t4\tfree single integer\n");
 printf("\t5\tmalloc blocks\n");
 printf("\t6\tfree last\n");
 printf("\t7\tfree random\n");
}

void free_interactive(){
 int input = 0;
 printf("Enter integer # to free: ");
 scanf("%d", &input);
 printf("Freeing tmp[%d]\n", input);
 mm_free(tmp[input]);
}


void free_random(){
 int input = (rand() % (last_used+1)); 
 printf("Freeing tmp[%d]\n", input);
 mm_free(tmp[input]);
}

int process(int arg){
 switch(arg) {
	case 1:
		options();	
	break;
	case 2:
		exit_status();
	break;
 	case 3:
		last_used++;
		test_single(last_used);
		exit_status();
	break;
	case 4:
		free_interactive();
		exit_status();
	break;
	case 5:
		last_used++;
		test_block(last_used);	
		exit_status();
	break;
	case 6:
		free_single(last_used);
		last_used--;
		exit_status();
	break;
	case 7:
		free_random();
		exit_status();
	break;
	default:
		exit_status();
 }
 return 0;
}


int get_user(){
 int input = 0;
 printf("\n>\t");
 scanf("%d", &input);
 return input;
}

int free_single(int index){
 mm_free(tmp[index]);
 return 0; 
}

int test_single( int index )
{
 tmp[index] = mm_malloc(sizeof(int));
 if(tmp[index]==NULL) return 1; 
 return 0;
}

int test_block (int index )
{
 tmp[index] = mm_malloc(sizeof(int)*BLKSZ);
 if(tmp[index]==NULL) return 1;
 return 0;
}
