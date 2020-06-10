/* execute.c - code used by small shell to execute commands */

#include	<stdio.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<signal.h>
#include	<sys/wait.h>	
#include	<sys/stat.h>
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

	int * FInPos  = (int*)malloc(sizeof(int));
	int FInSize   = 0;
	

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
		int	pid;
		int	child_info  = -1;
		
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
				Fptr = open(argv[FOutPos[0]+1], O_WRONLY | O_CREAT, S_IRWXU);
				dup2(Fptr,STDOUT_FILENO);
				
				execvp(argv[0], argv);
			}
			
			if (isFIn == 1 && isFOut == 0)
			{
				argv[1] = NULL;
				Fptr = open(argv[FInPos[0]+1], O_RDONLY | O_CREAT, S_IRWXU);
				dup2(Fptr,STDIN_FILENO);

				execvp(argv[0], argv);
			}

			if (isFIn == 1 && isFOut == 1)
			{
				char ** temp = (char**)malloc( 3 * sizeof(char*) );
				if (FInPos[0] < FOutPos[0])
				{
					Fptr = open(argv[FOutPos[0]+1], O_WRONLY | O_CREAT, S_IRWXU);
					dup2(Fptr,STDOUT_FILENO);

					temp[0] = "cat";
					temp[1] = argv[FInPos[0]+1];
					temp[2] = NULL;
			
				}
				else
				{
					Fptr = open(argv[FInPos[0]+1], O_WRONLY | O_CREAT, S_IRWXU);
					dup2(Fptr,STDOUT_FILENO);

					temp[0] = "cat";
					temp[1] = argv[FOutPos[0]+1];
					temp[2] = NULL;
				}

				execvp(temp[0],temp);
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
		
		int alternate 	= 0;
		int nextFIn     = 0;
		int nextFOut    = 0;

		char ** commands;
	
		for (int i = 0; i < pipeSize + 1; i++)
		{

			int delete  = 0;
			int hasFOut = 0;
			int hasFIn  = 0;

			int process = fork();

			if (process == 0)
			{
				if (i == 0)
				{
					//printf("First command\n");
					commands = (char**)malloc( (pipePos[0] * sizeof(char*)) + sizeof(char*) );
					
					for (int j = 0; j < pipePos[0]; j++)
					{
						if (*argv[j] == '<')
						{
							hasFIn = 1;
						}
				
						if (*argv[j] == '>')
						{
							hasFOut = 1;
						}
						commands[j] = argv[j];
						delete++;
					}
					commands[pipePos[0]] = NULL;

					dup2(firstPipe[1],STDOUT_FILENO);
					close(firstPipe[0]);

					if (hasFIn == 1)
					{
						commands[FInPos[0]] = NULL;
						Fptr = open(commands[pipePos[0]-1], O_RDONLY | O_CREAT, S_IRWXU);
						dup2(Fptr,STDIN_FILENO);
					}
					
					execvp(commands[0],commands);
				}
				
				if (i > 0 && i + 1 != pipeSize + 1)
				{
					//printf("Mid command\n");
					commands = (char**)malloc( ( ((pipePos[i] - 1) - pipePos[i - 1]) * sizeof(char*) ) + sizeof(char*) );

					for (int j = 0; j < pipePos[i] - pipePos[i - 1] - 1; j++)
					{
						if (*argv[j] == '<')
						{
							hasFIn  = 1;
						}
				
						if (*argv[j] == '>')
						{
							hasFOut  = 1;
						}

						commands[j] = argv[pipePos[i - 1] + j + 1];
						delete++;
					}
					
					for (int j = 0; j < pipePos[i]; j++)
					{
						if (*argv[j] == '<')
						{
							nextFIn++;
						}
				
						if (*argv[j] == '>')
						{
							nextFOut++;
						}
					}

					if (hasFIn == 1 && hasFOut == 0)
					{
						//printf("%d - %d = %d\n",pipePos[i],FInPos[nextFIn-1],(pipePos[i]) - FInPos[nextFIn-2]);
						//printf("FIn = %d\n",nextFIn-1);
						//printf("cell: %d has: %s\n",(pipePos[i]) - FInPos[nextFIn-1],commands[(pipePos[i]) - FInPos[nextFIn-1]]);
							
						commands[(pipePos[i] - 1) - FInPos[nextFIn-1]] = NULL;
						
						Fptr = open(commands[(pipePos[i]) - FInPos[nextFIn-1]], O_RDONLY | O_CREAT, S_IRWXU);
						dup2(Fptr,STDIN_FILENO);
					}

					if (hasFIn == 0 && hasFOut == 1)
					{
						//printf("%d - %d = %d\n",pipePos[i],FInPos[nextFIn-1],(pipePos[i]) - FInPos[nextFIn-2]);
						//printf("FOut = %d\n",nextFIn-1);
						//printf("cell: %d has: %s\n",(pipePos[i]) - FInPos[nextFIn-1],commands[(pipePos[i]) - FInPos[nextFIn-1]]);
							
						commands[(pipePos[i] - 1) - FOutPos[nextFOut-1]] = NULL;
						
						Fptr = open(commands[(pipePos[i]) - FOutPos[nextFOut-1]], O_RDONLY | O_CREAT, S_IRWXU);
						dup2(Fptr,STDOUT_FILENO);
					}

					if (hasFIn == 1 && hasFOut == 1)
					{
						char ** temp = (char**)malloc( 3 * sizeof(char*) );
						if (FInPos[nextFIn-1] < FOutPos[nextFOut-1])
						{
							Fptr = open(commands[(pipePos[i]) - FOutPos[nextFOut-1]], O_WRONLY | O_CREAT, S_IRWXU);
							dup2(Fptr,STDOUT_FILENO);

							temp[0] = "cat";
							temp[1] = commands[(pipePos[i]) - FInPos[nextFIn-1]];
							temp[2] = NULL;
					
						}
						else
						{
							Fptr = open(commands[(pipePos[i]) - FInPos[nextFIn-1]], O_WRONLY | O_CREAT, S_IRWXU);
							dup2(Fptr,STDOUT_FILENO);

							temp[0] = "cat";
							temp[1] = commands[(pipePos[i]) - FOutPos[nextFOut-1]];
							temp[2] = NULL;
						}

						execvp(temp[0],temp);
					}
					
					commands[pipePos[i] - pipePos[i - 1]] = NULL;

					if (alternate % 2 != 0)
					{
						if (hasFIn == 0)
						{
							dup2(firstPipe[0],STDIN_FILENO);
							close(firstPipe[1]);
						}
						
						if (hasFOut == 0)
						{
							dup2(secondPipe[1],STDOUT_FILENO);
							close(secondPipe[0]);
						}
						
						execvp(commands[0],commands);
					}
					else
					{
						if (hasFIn == 0)
						{
							dup2(secondPipe[0],STDIN_FILENO);
							close(secondPipe[1]);
						}

						if (hasFOut == 0)
						{
							dup2(firstPipe[1],STDOUT_FILENO);
							close(firstPipe[0]);
						}
						
						execvp(commands[0],commands);
					}
				}
				
				if (i + 1 == pipeSize + 1)
				{
					//printf("Last command\n");
					commands = (char**)malloc( ( ((length) - pipePos[i - 1]) * sizeof(char*) ) + sizeof(char*) );

					for (int j = 0; j < (length - 1) - pipePos[i - 1]; j++)
					{
						if (*argv[j] == '<')
						{
							hasFIn = 1;
							nextFIn++;
						}
				
						if (*argv[j] == '>')
						{
							hasFOut = 1;
							nextFOut++;
						}
						commands[j] = argv[pipePos[i - 1] + j + 1];
						delete++;
					}

					commands[(length) - pipePos[i - 1] - 1] = NULL;

					if (hasFOut == 1)
					{       
						commands[(length) - pipePos[i - 1] - 3] = NULL;
						Fptr = open(commands[(length) - pipePos[i - 1] - 2], O_WRONLY | O_CREAT, S_IRWXU);
						dup2(Fptr,STDOUT_FILENO);
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