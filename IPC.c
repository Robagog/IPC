#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>


struct message{
    long mtype;
    int mtext;
};

struct data{
    char line_shm[1024];
    int c1status;
    int c2status;
    int pstatus;
};

int main(int argc, char* argv[]){
    int er;
    if(argc != 2) return 1;
    FILE* f = fopen(argv[1], "r");
    pid_t p;
    key_t key1 = ftok(argv[0], 91);
    key_t key2 = ftok(argv[0], 92);
    size_t len = 0;
    char* line = NULL;
    struct data* mem;
    struct message* _msg;
    _msg = malloc(sizeof(struct message));
    
    int id = shmget(key1, sizeof(struct data), 0666|IPC_CREAT);
    int msgid = msgget(key2, IPC_CREAT|0666);
    if (id < 0) {
        perror("shmget");
        return 1;
    }
    mem = shmat(id, 0, 0);
    if ((int)mem == -1) {
        perror("shmat");
        shmctl(id, IPC_RMID, 0);
        shmdt(mem);
        return 1;
    }
    mem->pstatus = 0;
    mem->c1status = 1;
    mem->c2status = 1;

    if((p = fork()) == -1) {
        
        perror("Fork failed");
        shmctl(id, IPC_RMID, 0);
        shmdt(mem);
        return 1;
    }

    if (p == 0) {
        int i = 0;
        int cnt = 0;
        char* l;
        int msgid = msgget(key2,0);
        if(msgid == -1) perror("msgget: ");
        struct message* _msg;
        _msg = malloc(sizeof(struct message));
        while(1){
            if(mem->c1status == 0){   
                l = mem->line_shm;
                while(l[i] != '\0'){
                    if(l[i] == ' ') cnt++;
                    i++;
                }
                mem->c1status = 1;
                _msg->mtext = cnt;  
                _msg->mtype = 1;
                int k = msgsnd(msgid, _msg, sizeof(int), 0);
                if(k == -1) perror("msgnd: ");
                cnt = 0;
                i = 0;
                l = NULL;
            }
            if(mem->pstatus) break;
        }
        shmdt(mem);
        free(_msg);
        exit(0);
    }
    if((p = fork()) == -1) {
        perror("Fork failed");
        shmctl(id, IPC_RMID, 0);
        shmdt(mem);
        return 1;
    }

    if (p == 0) {
        int i = 0;
        int cnt = 0;
        char* l;
        struct message* _msg;
        _msg = malloc(sizeof(struct message));
        while(1){
            if(mem->c2status == 0){
                l = mem->line_shm;
                while(l[i] != '\0'){
                    if(l[i] >= '0' && l[i] <= '9') cnt++;
                    i++;
                }
                mem->c2status = 1;
                _msg->mtext = cnt;  
                _msg->mtype = 2;
                int k = msgsnd(msgid, _msg, sizeof(int), 0);
                if(k == -1) perror("msgnd: ");
                cnt = 0;
                i = 0;
                l = NULL;
                
            }
            if(mem->pstatus) break;
        }
        shmdt(mem);
        free(_msg);
        exit(0);
    }

    if((p = fork()) == -1) {
        perror("Fork failed");
        shmctl(id, IPC_RMID, 0);
        shmdt(mem);
        return 1;
    } 
    if (p == 0) {
        int num = 1;
        struct message* _msg;
        _msg = malloc(sizeof(struct message));
        while(1){
            int k = msgrcv(msgid, _msg, sizeof(int), 1, 0);
            if(k == -1) perror("msgrcv: ");
            if(_msg->mtext == -1) break;
            printf("%d --- ", num);
            printf("'_': %d; ", _msg->mtext);
            int k2 = msgrcv(msgid, _msg, sizeof(int), 2, 0);
            if(k2 == -1) perror("msgrcv: ");
            printf("'0-9': %d", _msg->mtext);
            printf("\n");
            num ++;
        }
        shmdt(mem);
        free(_msg);
        exit(0);
    }
    er = getline(&line, &len, f);
    if(er <= 0){
        mem->pstatus = 1;
        _msg->mtext = -1;  
        _msg->mtype = 1;
        int k = msgsnd(msgid, _msg, sizeof(int), 0);
        if(k == -1) perror("msgnd: ");
        return 1;
    } 
    while(1){
        if(mem->c1status == 1 && mem->c2status == 1){
            if(er <= 0){
                mem->pstatus = 1;
                _msg->mtext = -1;  
                _msg->mtype = 1;
                int k = msgsnd(msgid, _msg, sizeof(int), 0);
                if(k == -1) perror("msgnd: ");
                break;
            }
            strncpy(mem->line_shm, line, len);
            er = getline(&line, &len, f);
            mem->c1status = 0;
            mem->c2status = 0;
        }
    }
    wait(NULL);
    wait(NULL);
    wait(NULL);
    free(line);
    free(_msg);
    shmctl(id, IPC_RMID, 0);
    msgctl(msgid, IPC_RMID, NULL);
    shmdt(mem);
    return 0;
}