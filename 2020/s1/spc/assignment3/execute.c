/* execute.c - code used by small shell to execute commands */

#include	<stdio.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<signal.h>
#include	<sys/wait.h>	
#include	<fcntl.h>

/*
 * purpose: run a program passing it arguments
 * returns: status returned via wait, or -1 on error
 *  errors: -1 on fork() or wait() errors
 */

int execute(char *argv[])
{
	int length    = 0;
	int isPipe    = 0;
	//int isFIn     = 0;
	int isFOut    = 0;

	int * pipePos = (int*)malloc(sizeof(int));
	int pipeSize  = 0;

	int * FOutPos = (int*)malloc(sizeof(int));
	int FOutSize  = 0;

	int Fptr;

	while (argv[length] != NULL)
	{
		if (*argv[length] == '|')
		{
			isPipe = 1;
			pipePos = (int*)realloc(pipePos,sizeof(int)*(pipeSize + 1));
			pipePos[pipeSize] = length;

			pipeSize++;
		}	

		if (*argv[length] == '>')
		{
			isFOut = 1;
			FOutPos = (int*)realloc(FOutPos,sizeof(int)*(FOutSize + 1));
			FOutPos[FOutSize] = length;

			FOutSize++;
		}
		/*
		if (*argv[length] == '<')
		{
			isFIn = 1;
		}*/

		length++;
	}

	if (isPipe == 0)
	{
		int	pid ;
		int	child_info = -1;
		
		//fails if no args
		if ( argv[0] == NULL )
		{
			return 0;
		}
		
		//create fork here
		if ( (pid = fork())  == -1 )
		{
			perror("fork");
		}
		else if ( pid == 0 )
		{
			signal(SIGINT, SIG_DFL);
			signal(SIGQUIT, SIG_DFL);
			
			if (isFOut == 1)
			{       
				argv[1] = NULL;
				Fptr = open(argv[FOutPos[0]+1], O_WRONLY | O_CREAT/*, S_IRWXU*/);
				dup2(Fptr,STDOUT_FILENO);
				
				execvp(argv[0], argv);
			}
			else
			{
				execvp(argv[0], argv);
			}

			perror("cannot execute command");
			exit(1);
		}
		else 
		{
			if ( wait(&child_info) == -1 )
			{
				perror("wait");
			}

			if (isFOut == 1)
			{
				dup2(STDOUT_FILENO,Fptr);
				close(Fptr);
			}		
		}
		
		return child_info;
	}
	else
	{
		int firstPipe[2];
		int secondPipe[2];
		
		pipe(firstPipe);
		pipe(secondPipe);
		
		int alternate = 0;

		char ** commands;
	
		for (int i = 0; i < pipeSize + 1; i++)
		{
			//printf("\n### next iteration\n\n");

			int delete = 0;
			int process = fork();

			if (process == 0)
			{
				if (i == 0)
				{
					//printf("First Piping\n");
					commands = (char**)malloc( (pipePos[0] * sizeof(char*)) + sizeof(char*) );
					//printf("%d - %d = %d\n",pipePos[0], 0, pipePos[0] - 0);

					for (int j = 0; j < pipePos[0]; j++)
					{
						commands[j] = argv[j];
						//printf("%s\n",commands[j]);
						delete++;
					}
					commands[pipePos[0]] = NULL;

					//piping and then execvp
					dup2(firstPipe[1],STDOUT_FILENO);
					close(firstPipe[0]);
					
					execvp(commands[0],commands);
				}
				
				if (i > 0 && i + 1 != pipeSize + 1)
				{
					//printf("Mid Piping\n");
					commands = (char**)malloc( ( ((pipePos[i] - 1) - pipePos[i - 1]) * sizeof(char*) ) + sizeof(char*) );
					//printf("%d - %d = %d\n",pipePos[i] - 1, pipePos[i - 1], (pipePos[i] - 1) - pipePos[i - 1]);

					for (int j = 0; j < pipePos[i] - pipePos[i - 1] - 1; j++)
					{
						commands[j] = argv[pipePos[i - 1] + j + 1];
						//printf("%s\n",commands[j]);
						delete++;
					}
					commands[pipePos[i] - pipePos[i - 1]] = NULL;

					if (alternate % 2 != 0)
					{
						dup2(firstPipe[0],STDIN_FILENO);
						close(firstPipe[1]);

						dup2(secondPipe[1],STDOUT_FILENO);
						close(secondPipe[0]);
						
						execvp(commands[0],commands);
					}
					else
					{
						dup2(secondPipe[0],STDIN_FILENO);
						close(secondPipe[1]);

						dup2(firstPipe[1],STDOUT_FILENO);
						close(firstPipe[0]);
						
						execvp(commands[0],commands);
					}
				}
				
				if (i + 1 == pipeSize + 1)
				{
					//printf("Last Piping\n");

					//printf("%d - %d = %d\n",(length) , pipePos[i - 1], (length) - pipePos[i - 1]);
					commands = (char**)malloc( ( ((length) - pipePos[i - 1]) * sizeof(char*) ) + sizeof(char*) );

					for (int j = 0; j < (length - 1) - pipePos[i - 1]; j++)
					{
						commands[j] = argv[pipePos[i - 1] + j + 1];
						//printf("%s\n",commands[j]);
						delete++;
					}

					commands[(length) - pipePos[i - 1]] = NULL;

					if (alternate % 2 != 0)
					{
						dup2(firstPipe[0],STDIN_FILENO);
						close(firstPipe[1]);
						
						execvp(commands[0],commands);
					}
					else
					{
						dup2(secondPipe[0],STDIN_FILENO);
						close(secondPipe[1]);
						
						execvp(commands[0],commands);
					}
				}				
			}
			else
			{
				wait(NULL);	

				if (i == 0)
				{
					dup2(STDOUT_FILENO,firstPipe[1]);
					alternate++;
				}
				if (i > 0 && i + 1 != pipeSize + 1)
				{
					if (alternate % 2 != 0)
					{
						dup2(STDIN_FILENO,firstPipe[0]);
						dup2(STDOUT_FILENO,secondPipe[1]);
						pipe(firstPipe);
					}
					else
					{
						dup2(STDIN_FILENO,secondPipe[0]);
						dup2(STDOUT_FILENO,firstPipe[1]);
						pipe(secondPipe);
					}
					alternate++;
				}
				if (i + 1 == pipeSize + 1)
				{
					if (alternate % 2 != 0)
					{
						dup2(STDIN_FILENO,firstPipe[0]);
						pipe(firstPipe);
					}
					else
					{
						dup2(STDIN_FILENO,secondPipe[0]);
						pipe(secondPipe);
					}
				}
			}	
		}
	}
}
