#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/shm.h>
#include <errno.h>
#include <assert.h>
#include <dirent.h>
#include <stdbool.h>
#include <ctype.h>

#include "helpers.h"

#define SHM_SIZE 1024*1024

/**
 * @brief This function recursively traverse the source directory.
 * 
 * @param dir_name : The source directory name.
 */
void traverseDir(char *dir_name);
void c_file(int wordcount);

int num_paths = 0;
int total = 0;
char File_arr[10][256];


typedef struct{
	sem_t sem_full; //Signal child that the parent has written data (initially 0)
	sem_t sem_empty; // Signals the parent that the cild has finished processes (initially 1)
	int file_index; // index of the file in File_arr
	int content_size; // size of the content
	char content[SHM_SIZE - 2 * sizeof(sem_t) - 2 * sizeof(int)]; // File content
}SharedMemory;

int main(int argc, char **argv) {
	int process_id; // Process identifier 
	
    // The source directory. 
    // It can contain the absolute path or relative path to the directory.
	char *dir_name = argv[1];
	int shmid;

	if (argc < 2) {
		printf("Main process: Please enter a source directory name.\nUsage: ./main <dir_name>\n");
		exit(-1);
	}
	
	traverseDir(dir_name);

    /////////////////////////////////////////////////
    // You can add some code here to prepare before fork.
    /////////////////////////////////////////////////
	
	shmid = shmget(IPC_PRIVATE, SHM_SIZE, 0666|IPC_CREAT);


	if (shmid == -1 ){
		printf("Creation of shared memory is failed");
		exit(-1);
	}

	SharedMemory *shared = (SharedMemory *)shmat(shmid, NULL, 0);

	if (sem_init(&shared->sem_full, 1, 0) == -1 || sem_init(&shared->sem_empty, 1, 1) == -1){
		printf("Semaphore initialization faild");
		exit(-1);
	}

	switch (process_id = fork()) {

	default:
		/*
			Parent Process
		*/
		printf("Parent process: My ID is %jd\n", (intmax_t) getpid());
        

        /////////////////////////////////////////////////
        // Implement your code for parent process Shere.
        /////////////////////////////////////////////////

		// size_t shm_offset = 0;

		for (int i = 0; i < num_paths; i++) {
			sem_wait(&shared->sem_empty);
			shared->file_index = i;

			FILE *file = fopen(File_arr[i], "r");
            if (!file) {
            	printf("Parent: Failed to open file %s: %s\n", File_arr[i], strerror(errno));
            	shared->content_size = 0;
			}else{
				size_t bytes_read = fread(shared->content, 1, sizeof(shared->content) - 1, file);
				if (ferror(file) || bytes_read > (SHM_SIZE - 2 * sizeof(sem_t) - 2 * sizeof(int))){
					printf("ParentL Error reading file %s or file size exceeds the buffer\n", File_arr[i]);
					shared->content_size = 0;
				}else{
					shared->content[bytes_read] = '\0';
					shared->content_size = (int)bytes_read;
				}
				fclose(file);
			}

			printf("Parent: Processed %s\n", File_arr[i]);
			sem_post(&shared->sem_full);

			printf("Parent: Wrote content of %s to shared memory\n", File_arr[i]);
		}

		//Signal child to terminate
		sem_wait(&shared->sem_empty);
		shared->file_index = -1;
		sem_post(&shared->sem_full);
		wait(NULL); // Wait for child to finish
		printf("Parent process: Finished.\n");
		sem_destroy(&shared->sem_full);
		sem_destroy(&shared->sem_empty);
		shmdt(shared);
		shmctl(shmid, IPC_RMID, NULL);
		break;


	case 0:
		/*
			Child Process
		*/
		

		printf("Child process: My ID is %jd\n", (intmax_t) getpid());

        /////////////////////////////////////////////////
        // Implement your code for child process here.
        /////////////////////////////////////////////////

		int idx = 1;
		while (1){
			sem_wait(&shared->sem_full);
			if (shared->file_index == -1) break;

			if (shared->content_size > 0){
				int word_count = wordCount(shared->content);
				printf("Child: Word count for %s: %d\n", File_arr[idx - 1], word_count);
				c_file(word_count);
				total += word_count;
				idx++;
			}else {
				printf("Child: No content for %s\n", File_arr[shared->file_index]);
			}
			sem_post(&shared->sem_empty); // signal parent that processing is done
		}


		printf("Child process: Finished.\n");

		saveResult("p2_result.txt", total);

		shmdt(shared);
		exit(0);

	case -1:
		/*
		Error occurred.
		*/
		printf("Fork failed!\n");
		exit(-1);
	}


	exit(0);
}

/**
 * @brief This function recursively traverse the source directory.
 * 
 * @param dir_name : The source directory name.
 */

// SID = 58533046 NG KA HUNG
const int stdLastDigit = 6;

void c_file(int wordcount){
	int file_digit = wordcount % 10;
	if (file_digit == stdLastDigit){
		printf("The last number of words in the file is equal to the last number of chosen student ID %d.\n", stdLastDigit);
	}else {
		printf("The last number of words in the file is not equal to the last number of chosen student ID %d.\n", stdLastDigit);

	}
}

void traverseDir(char *dir_name){
    
    // Implement your code here to find out
    // all textfiles in the source directory.

	DIR* dir;
    struct dirent *ent;
    struct stat states;
	char path[512];

    dir = opendir(dir_name);

    while((ent=readdir(dir)) != NULL){
        stat(ent->d_name,&states);
        if(!strcmp(".", ent->d_name) || !strcmp("..", ent->d_name)){
                    continue;
        }

        snprintf(path, sizeof(path), "%s/%s", dir_name, ent->d_name);


        if (stat(path, &states) == -1) {
            printf("Cannot stat %s: %s\n", path, strerror(errno));
            continue;
        }

        if (S_ISREG(states.st_mode)) {
        	if (num_paths < 10 && strstr(ent->d_name, ".txt")) {
           		strncpy(File_arr[num_paths], path, 255);
            	File_arr[num_paths][255] = '\0';
            	printf("Found file: %s\n", File_arr[num_paths]);
            	num_paths++;
        	}
    	} else if (S_ISDIR(states.st_mode)) {
          	traverseDir(path);
		}
    }
    closedir(dir);
}