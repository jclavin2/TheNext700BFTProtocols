#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "Timer.h"
#include <sys/time.h>
#define OWNER 1
extern "C"
{
#include "ldap_search.h"
}


int main(int argc, char **argv)
{

	int rc;
	char *result=NULL;
	char base[128]="";
	char filter[9000]="";
	strcpy(base,argv[1]);
	strcpy(filter,argv[2]);
	Timer timer;
	timer.start();	
	char filter1[9000];
	srand(time(NULL)+ getpid());
	//char ips_first[]="127.0.0.1";
	//char ips_sec[]="127.0.2.1";	
	// Set the number fo rounds(requests)
	const int ROUNDS_NB=10;
	const int DB_SIZE=100000;
	
	int first=0;
	//ldap_host=ips_first;
	for(int kk=0;kk<ROUNDS_NB;kk++)
	{
	/*if(!(kk%10000))
	{
	first=(first+1)%2;
	if(first)
		ldap_host=ips_first;
	else ldap_host=ips_sec;
	}*/

	char tmp[10];
	strcpy(filter1,filter);
	int ran=(rand() % DB_SIZE);
//fprintf(stderr,"Request is:user.%d\n",ran);


	sprintf(tmp,"%d",ran);
	strcat(filter1,tmp);
	fprintf(stderr,"\n filter is:%s\n",filter);
	rc=do_ldap_search(base,filter,&result);

	if(rc!= EXIT_SUCCESS)
	{
		fprintf(stderr,"\ntester.c: error in main(); do_ldap_search failed !!\n");
		return EXIT_FAILURE;
	}

	fprintf(stderr,"Result is *result:\n%s",result);

	if( result!=NULL && kk!=ROUNDS_NB-1)
	free(result);
	if(!(kk%1000))
	{
	fprintf(stderr,"\n Iteration %d; the response size is:%d;this is ldap_host:%s\n",kk,strlen(result)*sizeof(char),ldap_host);
	}
	}
	free(result);
	result=NULL;
	timer.stop();	
	fprintf(stderr,"\n Throughput:%f;  Elapsed time:%f;Response time=%f \n",ROUNDS_NB/timer.elapsed(),timer.elapsed(),timer.elapsed()/ROUNDS_NB);
	return EXIT_SUCCESS;
}
