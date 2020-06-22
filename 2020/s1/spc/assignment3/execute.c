/* execute.c - code used by small shell to execute commands */

#include	<stdio.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<signal.h>
#include	<sys/wait.h>
#include	<sys/stat.h>
#include	<fcntl.h>

/*
 * purpose: run a program passing it arguments, performs piping and redirection if needed
 * returns: status returned via wait, or -1 on error
 *  errors: -1 on fork() or wait() errors
 */

int execute(char *argv[])
{
	int length    = 0;
	int isPipe    = 0;
	int isFIn     = 0;
	int isFOut    = 0;

	//when to pipe and how many times to do it
	int * pipePos = (int*)malloc(sizeof(int));
	int pipeSize  = 0;

	//redirects output from terminal to a file, and how many times to do it
	int * FOutPos = (int*)malloc(sizeof(int));
	int FOutSize  = 0;

	//redirects input from terminal and gets input from file, and how many times to do it
	int * FInPos = (int*)malloc(sizeof(int));
	int FInSize  = 0;

	//file pointer to open files from
	int Fptr;
	int Fptr1;

	while (argv[length] != NULL)
	{
		//while loop that checks the input, counts the length of the input, and marks the amount and locations of < , > and |
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

	//conditional to check if a pipe is needed for redirecting output from previous command or not

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

			//runs command with output to a file
			if (isFOut == 1 && isFIn == 0)
			{
				argv[1] = NULL;
				Fptr = open(argv[FOutPos[0]+1], O_WRONLY | O_CREAT, S_IRWXU);
				dup2(Fptr,STDOUT_FILENO);

				execvp(argv[0], argv);
			}

			//runs command with input from a file
			if (isFIn == 1 && isFOut == 0)
			{
				argv[1] = NULL;
				Fptr = open(argv[FInPos[0]+1], O_RDONLY | O_CREAT, S_IRWXU);
				dup2(Fptr,STDIN_FILENO);

				execvp(argv[0], argv);
			}

			//runs command with input from a file then outputs to a file
			if (isFIn == 1 && isFOut == 1)
			{
				argv[1] = NULL;
				argv[3] = NULL;
				Fptr = open(argv[FInPos[0]+1], O_RDONLY | O_CREAT, S_IRWXU);
				Fptr1 = open(argv[FOutPos[0]+1], O_WRONLY | O_CREAT, S_IRWXU);
				dup2(Fptr,STDIN_FILENO);
				dup2(Fptr1,STDOUT_FILENO);

				execvp(argv[0], argv);
			}

			// runs command normally if no input or output to a file
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
	else //runs piping if argv needs it, works multiple times
	{
		//creating pipes needed for multiple pipings
		int firstPipe[2];
		int secondPipe[2];

		pipe(firstPipe);
		pipe(secondPipe);

		//alternate for switching between pipes as one is used for input and the other is used for output
		int alternate = 0;

		char ** commands;

		for (int i = 0; i < pipeSize + 1; i++)
		{
			//forking process
			int delete = 0;
			int process = fork();

			//child process
			if (process == 0)
			{
				//condition for when it is inital pipe, output to pipe for next pipe command
				if (i == 0)
				{
					commands = (char**)malloc( (pipePos[0] * sizeof(char*)) + sizeof(char*) );

					//setting the argv commands into the command for execvp
					for (int j = 0; j < pipePos[0]; j++)
					{
						commands[j] = argv[j];
						delete++;
					}
					commands[pipePos[0]] = NULL;

					dup2(firstPipe[1],STDOUT_FILENO);
					close(firstPipe[0]);

					//can take input from file if needed
					if (isFIn == 1)
					{
						commands[FInPos[0]] = NULL;
						Fptr = open(commands[pipePos[0]-1], O_RDONLY | O_CREAT, S_IRWXU);
						dup2(Fptr,STDIN_FILENO);
					}

					execvp(commands[0],commands);
				}

				//if command is the middle of the pipes, collect input from previous command and then output for next command
				if (i > 0 && i + 1 != pipeSize + 1)
				{
					commands = (char**)malloc( ( ((pipePos[i] - 1) - pipePos[i - 1]) * sizeof(char*) ) + sizeof(char*) );

					//setting the argv commands into the command for execvp
					for (int j = 0; j < pipePos[i] - pipePos[i - 1] - 1; j++)
					{
						commands[j] = argv[pipePos[i - 1] + j + 1];
						delete++;
					}
					commands[pipePos[i] - pipePos[i - 1]] = NULL;

					//alternates to see which pipe the last command's output is
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


				//final pipe condition, gets the input from the previous command
				if (i + 1 == pipeSize + 1)
				{
					commands = (char**)malloc( ( ((length) - pipePos[i - 1]) * sizeof(char*) ) + sizeof(char*) );

					//setting the argv commands into the command for execvp
					for (int j = 0; j < (length - 1) - pipePos[i - 1]; j++)
					{
						commands[j] = argv[pipePos[i - 1] + j + 1];
						delete++;
					}

					commands[(length) - pipePos[i - 1]] = NULL;

					//lets the command output to a file if needed
					if (isFOut == 1)
					{
						commands[(length) - pipePos[i - 1] - 3] = NULL;
						Fptr = open(commands[(length) - pipePos[i - 1] - 2], O_WRONLY | O_CREAT, S_IRWXU);
						dup2(Fptr,STDOUT_FILENO);
					}

					//alternates to see which pipe the last command's output is
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

				//resets the piping based on alternating sequence and piping conditions
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