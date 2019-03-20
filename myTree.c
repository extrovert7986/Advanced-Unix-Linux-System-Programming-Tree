#include<stdio.h>
#include<dirent.h>
#include<sys/stat.h>
#include<string.h>
#include<stdbool.h>
#include<fcntl.h>
#include<stdlib.h>
#include<unistd.h>
/*****************************
  This program is used for listing all file under target
directory in tree structure.
  Providing several function setting to user include:
    1.-a: list all files include hidden file
    2.-L num: list tree structure until the depth num
    3.-f: print all full prefix
    4.-h: print the file size in front of each file
******************************/
int curLV = -1;
int MAXLV = 16;
int dirCnt = 0;
int fileCnt = 0;
int slkCnt = 0;
int totalSize = 0; int fileSize;
unsigned char control = 0;

enum TYPE {REG,DIRECTORY,CHR,BLK,FIFO,SLNK,SOCK };

void print_tree(const char*,int);
int check_file_type(const char*);

int main(int argc,char** argv){
    struct stat* buf = (struct stat*)malloc(sizeof(struct stat));
    int cmdCnt,stridx;
    int treeCnt = 0;
    int idx;
    char* dirToOpen[256];
    
    for( cmdCnt = 1; cmdCnt < argc; cmdCnt++){
        if(!strcmp(argv[cmdCnt],"-a")){
            control |= 1;
            continue;
        }
        else if(!strcmp(argv[cmdCnt],"-L")){
            if(cmdCnt == argc-1) {
                printf("Missing argument to -L option.\n");
                return 0;
            }
            cmdCnt++;
            MAXLV = 0;
            for(stridx = strlen(argv[cmdCnt])-1; stridx>=0; stridx--){
                if(argv[cmdCnt][stridx]>='0'&&argv[cmdCnt][stridx]<='9')
                    MAXLV= MAXLV*10+(argv[cmdCnt][stridx]-'0');
                else{
                    printf("Invalid level, must be greater than 0.\n");
                    return 0; 
                }
            }
            continue;
        }
        else if(!strcmp(argv[cmdCnt],"-f")){
            control |= 2;
            continue;
        }
        else if(!strcmp(argv[cmdCnt],"-h")){
            control |= 4;
            continue;
        }
        else {
            dirToOpen[treeCnt++] = argv[cmdCnt];
        }
    }
    if(treeCnt==0){
        print_tree(".",0);
    }
    else
        for(idx=0;idx<treeCnt;idx++){
            if(DIRECTORY != check_file_type(dirToOpen[idx])){
                printf("%s [error opening dir]\n\n",dirToOpen[idx]);
                continue;    
            }
            print_tree(dirToOpen[idx],0);
            printf("\n");
        }

    printf("%d directories, %d files, %d soft links.\n",dirCnt,fileCnt,slkCnt);
    printf("Block size: %d Byte\n", totalSize);
    return 0;
}

int hidden_filter(const struct dirent*);
int hidden_DIRONLY_filter(const struct dirent*);
int dot_dotdot_filter(const struct dirent*);
int dot_dotdot_DIRONLY_filter(const struct dirent*);

int compar(const struct dirent**,const struct dirent**);
void print_name(const char*);

void print_tree(const char* pathName,int branchStat){
    struct dirent** dirList;
    int numOfDir;
    int fileType;
    int i,lv;
    char* tmp;
    
    curLV++;
    //get dirent in the directory
    if(control & 1)
        numOfDir = scandir(pathName,&dirList,dot_dotdot_filter,compar);    
    else
        numOfDir = scandir(pathName,&dirList,hidden_filter,compar);
    
    //print file name
    if(curLV)
        print_name(pathName);
    else printf("\033[34;1m%s\033[0m\n",pathName);
    
    if(curLV<MAXLV){
        for(i = 0; i < numOfDir; i++){
            for(lv = 0; lv < curLV; lv++){
                if((1<<lv)&branchStat)
                    printf("    ");
                else
                    printf("┃   ");
                }
            if(i==numOfDir-1){
                branchStat |= (1<<curLV);
                printf("┗━━ ");
            }
            else
                printf("┣━━ ");
            //string processing for next level(layer)
            tmp = (char*)calloc(strlen(pathName)+strlen(dirList[i]->d_name)+2,sizeof(char));
            if(!tmp){
                printf("alloc name failed.\n");
            }
            strcpy(tmp,pathName);
            strcat(tmp,"/");
            strcat(tmp,dirList[i]->d_name);
            
            print_tree(tmp,branchStat);
            
            free(tmp);
            free(dirList[i]);
            if(i == numOfDir-1)
                free(dirList);
        }
    }
    curLV--;
}

int compar(const struct dirent** dir1,const struct dirent** dir2){
    return strcmp((*dir1)->d_name,(*dir2)->d_name);
}

int hidden_filter(const struct dirent* dir){
    if(dir->d_name[0]=='.')
        return 0;
    return 1;
} 

int dot_dotdot_filter(const struct dirent* dir){
    if(strcmp(dir->d_name,".")&&strcmp(dir->d_name,".."))
        return 1;   
    else return 0;
}

void print_name(const char* pathName){
    char buf[200] = {0};
    char* out = (char*)calloc((strlen(pathName)+1),sizeof(char));
    char* lastName;
    if(!(control&2))
        strcpy(out, strrchr(pathName,'/')+1);
    else strcpy(out, pathName);
    
    if(control & 4)
        printf("[ %d ] ",fileSize);
        
    switch(check_file_type(pathName)){
    case REG:
        printf("%s\n", out);
        fileCnt++;
        break;
    case DIRECTORY:
        printf("\033[34;1m%s\033[0m\n",out);
        dirCnt++;
        break;
    case CHR:
        printf("\033[44m%s\033[0m\n",out);
        fileCnt++;
        break;       
    case BLK:
        printf("%s\n",out);
        fileCnt++;
        break;
    case FIFO:
        printf("%s\n",out);
        fileCnt++;
        break;
    case SLNK:
        printf("\033[33;1m%s\033[0m -> ",out);
        readlink(pathName,buf,200);
        
        out = (char*)realloc(out,strlen(pathName)+strlen(buf)+1);
        strcpy(out, pathName);
        lastName = strrchr(out,'/');
        if(buf[0]=='/'){
            strcpy(out,buf);
        }
        else strcpy(lastName+1,buf);
        if(buf[strlen(buf)] == '/');
            buf[strlen(buf)] = '\0';
        
        if(DIRECTORY==check_file_type(out)){
            if(!opendir(out))
                printf("%s doesn't exist.\n", strrchr(out,'/')+1);
            else
                print_name(out);
        }
        else {
            if(open(out,O_RDONLY) == -1)
                printf("%s doesn't exist.\n", strrchr(out,'/')+1);
            else    
                print_name(out);
        }
        slkCnt++;
        break;
    case SOCK:
        printf("%s\n",out);
        fileCnt++;
        break;
    default:
        printf("%s\n",out);
        fileCnt++;
        break;
    }
    free(out);

}

int check_file_type(const char* pathName){
    struct stat st;
    int rtVal;
    if(-1 == lstat(pathName,&st)){
        rtVal -1;
    }
    
    if(S_ISREG(st.st_mode))
        rtVal = 0;
    else if(S_ISDIR(st.st_mode))
        rtVal = 1;
    else if(S_ISCHR(st.st_mode))
        rtVal = 2;
    else if(S_ISBLK(st.st_mode))
        rtVal = 3;
    else if(S_ISFIFO(st.st_mode))
        rtVal = 4;
    else if(S_ISLNK(st.st_mode))
        rtVal = 5;
    else if(S_ISSOCK(st.st_mode))
        rtVal = 6;
    else rtVal = -1;
    fileSize = st.st_size;
    totalSize += st.st_size;
    
    return rtVal; 
}
