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
	int isFIn     = 0;
	int isFOut    = 0;

	int * pipePos = (int*)malloc(sizeof(int));
	int pipeSize  = 0;

	int * FOutPos = (int*)malloc(sizeof(int));
	int FOutSize  = 0;

	int * FInPos = (int*)malloc(sizeof(int));
	int FInSize  = 0;

	int Fptr;
	int Fptr2;

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
		
		if (*argv[length] == '<')
		{
			isFIn = 1;
			FInPos = (int*)realloc(FInPos,sizeof(int)*(FInSize + 1));
			FInPos[FInSize] = length;

			FInSize++;
		}

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
			
			if (isFOut == 1 && isFIn == 0)
			{       
				argv[1] = NULL;
				Fptr = open(argv[FOutPos[0]+1], O_WRONLY | O_CREAT/*, S_IRWXU*/);
				dup2(Fptr,STDOUT_FILENO);
				
				execvp(argv[0], argv);
			}
			
			if (isFIn == 1 && isFOut == 0)
			{
				argv[1] = NULL;
				Fptr = open(argv[FInPos[0]+1], O_RDONLY | O_CREAT/*, S_IRWXU*/);
				dup2(Fptr,STDIN_FILENO);

				execvp(argv[0], argv);
			}

			execvp(argv[0], argv);

			perror("cannot execute command");
			exit(1);
		}
		else 
		{
			if ( wait(&child_info) == -1 )
			{
				perror("wait");
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

			int delete = 0;
			int process = fork();

			if (process == 0)
			{
				if (i == 0)
				{
					commands = (char**)malloc( (pipePos[0] * sizeof(char*)) + sizeof(char*) );
					
					for (int j = 0; j < pipePos[0]; j++)
					{
						commands[j] = argv[j];

						delete++;
					}
					commands[pipePos[0]] = NULL;

					dup2(firstPipe[1],STDOUT_FILENO);
					close(firstPipe[0]);

					if (isFIn == 1)
					{
						commands[FInPos[0]] = NULL;
						Fptr = open(commands[pipePos[0]-1], O_RDONLY | O_CREAT/*, S_IRWXU*/);
						dup2(Fptr,STDIN_FILENO);
					}
					
					execvp(commands[0],commands);
				}
				
				if (i > 0 && i + 1 != pipeSize + 1)
				{

					commands = (char**)malloc( ( ((pipePos[i] - 1) - pipePos[i - 1]) * sizeof(char*) ) + sizeof(char*) );


					for (int j = 0; j < pipePos[i] - pipePos[i - 1] - 1; j++)
					{
						commands[j] = argv[pipePos[i - 1] + j + 1];
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
					commands = (char**)malloc( ( ((length) - pipePos[i - 1]) * sizeof(char*) ) + sizeof(char*) );

					for (int j = 0; j < (length - 1) - pipePos[i - 1]; j++)
					{
						commands[j] = argv[pipePos[i - 1] + j + 1];

						delete++;
					}

					commands[(length) - pipePos[i - 1]] = NULL;

					if (isFOut == 1)
					{       
						commands[(length) - pipePos[i - 1] - 3] = NULL;
						Fptr2 = open(commands[FOutPos[0]+1], O_WRONLY | O_CREAT/*, S_IRWXU*/);
						dup2(Fptr2,STDOUT_FILENO);
					}

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
