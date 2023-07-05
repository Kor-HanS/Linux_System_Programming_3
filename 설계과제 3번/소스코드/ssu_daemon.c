#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "ssu_monitor.h"
#include "ssu_daemon.h"

node* get_node(const char *path,  pid_t pid) {
	// 다른 노드함수 들과 달리, 디몬 프로세스가 관리하는 노드를 만들어 반환한다.
	node* new_node = (node*)malloc(sizeof(node));

	memset(new_node->path,0,sizeof(new_node->path));
	strcpy(new_node->path,path);
	new_node->pid = pid;
	new_node->next = NULL;
	new_node->child = NULL;
	
	if(access(path, F_OK) < 0)
		mkdir(path, 0775);
	
	make_tree(&new_node->child,path);
	
	return new_node;
}

void initialize_nodes(node *monitoring_node) {
	if(monitoring_node == NULL)
		return;

	node_sub *now_node = monitoring_node->child;

	if(now_node != NULL) // 현재 디몬 프로세스가 관리하는 최상위 디렉토리 하위의 파일 및 서브 디렉토리 노드 (node_sub) 의 상태를 초기화 합니다.
		initialize_node(now_node); 
}

void initialize_node(node_sub *now_node) {
	now_node->state = UNCHECKED;

	if(now_node->child != NULL) // 해당 노드의 자식 노드 초기화를 위해 재귀적 함수 호출.
		initialize_node(now_node->child);
	
	if(now_node->next != NULL) // 해당 노드의 같은 레벨 형제 노드 초기화를 위해 재귀적 함수 호출.
		initialize_node(now_node->next);
}

void compare_node(node *old_node, node *new_node) {
	int fd;
	char log_txt[BUFFER_SIZE];
	sprintf(log_txt,"%s/%s",getenv("PWD"),LOG_TXT); // LOG_TXT : "log.txt" 현재 프로그램을 실행 한 공간에 log.txt를 만들어서 로그를 저장한다.
	
	if((fd = open(log_txt, O_WRONLY | O_CREAT | O_APPEND, 0664)) < 0){
		fprintf(stderr, "open error for %s\n",log_txt);
		exit(1);
	}

	if(old_node == NULL || new_node == NULL)
		return;

	if(new_node->child != NULL)
		compare_node_sub(fd, old_node->child, new_node->child); // 새로운 노드가 비어있을경우 무시한다. -> old_node가 UNCHECKED라면 어차피 삭제 된 처리 임.
	
	check_node_deleted(fd, old_node->child); // old_node->child 인 node_sub 노드들중 state 가 UNCHECKED 라면 삭제된 노드 이므로, 삭제 로그 찍기.
	close(fd);
}

void compare_node_sub(int fd, node_sub *old_node_head, node_sub *new_node_sub) { // old_node 트리를 탐색한다.
	char log_string[BUFFER_SIZE]; // make_time_string() + [바뀌었다면 비교 결과] + [경로]
	char time_string[30];
	char state_string[20];
	int is_path_valid = TRUE;
	memset(log_string,0,sizeof(log_string));

	// 디렉토리가 아닌 파일 타입의 노드 일 경우, 모니터링 대상.
	if(new_node_sub->type == TYPE_FILE) {
		compare_info(old_node_head, new_node_sub); 
		if(realpath(new_node_sub->path, NULL) == NULL) { // 접근 권한 없는 노드 일 경우 지나치기.
			is_path_valid = FALSE; // 접근 권한이 없는 경로 일 경우 서브 디렉토리 및 하위 파일을 탐색하지 않는다.
			if(new_node_sub->next != NULL)
				compare_node_sub(fd, old_node_head, new_node_sub->next);
		}
		if(new_node_sub->state == UNCHECKED) {
			new_node_sub->state = CREATED;
			get_time_string(time_string);
			get_state_string(new_node_sub->state,state_string);
			sprintf(log_string, "%s%s[%s]\n",time_string,state_string,new_node_sub->path);
			write(fd,log_string,strlen(log_string));
			new_node_sub->state = CHECKED;
		} else if(new_node_sub->state == MODIFIED) {
			get_time_string(time_string);
			get_state_string(new_node_sub->state,state_string);
			sprintf(log_string, "%s%s[%s]\n",time_string,state_string,new_node_sub->path);
			write(fd,log_string,strlen(log_string));
			new_node_sub->state = CHECKED;
		}
	}else if(new_node_sub->type == TYPE_DIR) {
		if(realpath(new_node_sub->path, NULL) == NULL) { // 접근 권한 없는 노드 일 경우 지나치기.
			is_path_valid = FALSE; // 접근 권한이 없는 경로 일 경우 서브 디렉토리 및 하위 파일을 탐색하지 않는다.
			if(new_node_sub->next != NULL)
				compare_node_sub(fd, old_node_head, new_node_sub->next);
		}
	}

	if(is_path_valid) {
	// 비교가 정상 적으로 이루어 진 후 넘어가기.
		if(new_node_sub->child != NULL)
			compare_node_sub(fd, old_node_head, new_node_sub->child);
		if(new_node_sub->next != NULL)
			compare_node_sub(fd, old_node_head, new_node_sub->next);
	}
}

void compare_info(node_sub *old_node, node_sub *new_node) {
	// 두 노드의 상태를 초기화 한다.
	// 수정 상태 일 경우 두 노드 모두 MODIFIED 
	// 새로 생성 상태 일 경우. new_node 측 CREATED 
	// 삭제 된 노드 일경우, 예전 노드 측 UNCHECKED == DELETED
	// 이때 new_node 인자는 무조건 파일 노드이다. compare_node_sub() 에서 디렉토리 일경우 해당 함수 호출 하지 않음.
	int is_path_valid = TRUE;

	if(old_node == NULL || new_node == NULL)
		return;

	if(realpath(old_node->path, NULL) == NULL)  { 
		is_path_valid = FALSE;
		compare_info(old_node->next, new_node);
	}

	if(is_path_valid) { 
		if(old_node->child != NULL)
			compare_info(old_node->child, new_node);
	
		if(old_node->next != NULL)
			compare_info(old_node->next, new_node);
	}

	// 두 파일이 같을 경우.
	if(old_node->type == TYPE_FILE && !strcmp(old_node->path,new_node->path) && !strcmp(old_node->name,new_node->name))  {
		if(old_node->node_mtime == new_node->node_mtime) {
			old_node->state = CHECKED;
			new_node->state = CHECKED;
			return;
		}else {
			old_node->state = CHECKED;
			new_node->state = MODIFIED;
			return;
		}
	}
}

void check_node_deleted(int fd, node_sub *old_node) {
	char log_string[BUFFER_SIZE]; // make_time_string() + [바뀌었다면 비교 결과] + [경로]
	char time_string[30];
	char state_string[20];
	memset(log_string,0,sizeof(log_string));
	
	if(old_node == NULL)
		return;

	if(old_node-> child != NULL) 
		check_node_deleted(fd, old_node->child);

	if(old_node->state == UNCHECKED && old_node->type == TYPE_FILE){ // 파일의 삭제에 대해 모니터링.
		old_node->state = DELETED; // DELETED 상태로 변경.
		get_time_string(time_string);
		get_state_string(old_node->state,state_string);
		sprintf(log_string, "%s%s[%s]\n",time_string,state_string,old_node->path);
		write(fd,log_string,strlen(log_string));
		old_node->state = CHECKED; // 로그를 찍었으므로, 상태를 CHECKED 상태로 변경.
	}

	if(old_node->next != NULL)
		check_node_deleted(fd, old_node->next);
}

// node 하위 node_sub 노드들로 이루어진 파일 트리를 만들기 위한 함수.
void make_tree(node_sub **head, const char *path) {
	DIR *dir;
	char tmp[BUFFER_SIZE];
	struct stat statbuf;
	struct dirent *dirp;
	node_sub *now_node;

	if(realpath(path,NULL) == NULL) 
		return;

	if((dir = opendir(path)) == NULL)
		return;

	while((dirp = readdir(dir)) != NULL) {
		if(!strcmp(dirp->d_name,".") || !strcmp(dirp->d_name,".."))
			continue;

		strcpy(tmp,path);
		strcat(tmp,"/");
		strcat(tmp,dirp->d_name);
		
		if(lstat(tmp, &statbuf) < 0) 
			continue;

		if(S_ISDIR(statbuf.st_mode)) {
			now_node = push_back_node_sub(head, tmp, dirp->d_name); // head의 마지막에 디렉토리의 노드를 추가.
			if(now_node == NULL)
				continue;
			make_tree(&now_node->child,tmp); // 현재 서브 디렉토리 노드의 child를 head로 하여서 하위 서브 디렉토리 및 파일의 노드를 추가한다.
		} else if(S_ISREG(statbuf.st_mode)){
			push_back_node_sub(head, tmp, dirp->d_name); // head의 마지막에 파일 노드를 추가.
		}
	}

	if(dir != NULL)
		closedir(dir);
}

void print_tree(node_sub **head, int depth) {
	int i;
	node_sub *now_node = *head;
	
	while(now_node != NULL) {
		// 본인 출력.
		printf("|");
		for(i = 1 ; i <= depth*3; i++)
			printf("  ");
		printf("|--- %s\n",now_node->name);
		// 자식 출력.
		if(now_node->child != NULL) {
			print_tree(&now_node->child, depth+1);
		}

		now_node = now_node->next;
	}
}

node_sub* push_back_node_sub(node_sub **head, char *path, char *name) {
	struct stat statbuf;

	node_sub *now_node = NULL;
	node_sub *new_node = (node_sub*)malloc(sizeof(node_sub));
	
	memset(new_node->path, 0, sizeof(new_node->path));
	memset(new_node->name, 0,sizeof(new_node->name));
	strcpy(new_node->path, path);
	strcpy(new_node->name, name);
	new_node->state = UNCHECKED;
	new_node->next = NULL;
	new_node->child = NULL;
	
	if(lstat(new_node->path, &statbuf) < 0) {
		memset(new_node->path, 0, sizeof(new_node->path));
		memset(new_node->name, 0,sizeof(new_node->name));
		free(new_node);
		new_node = NULL;
		return NULL;
	}

	new_node->node_mtime = statbuf.st_mtime;

	if(realpath(new_node->path, NULL) == NULL) {
		memset(new_node->path, 0, sizeof(new_node->path));
		memset(new_node->name, 0,sizeof(new_node->name));
		free(new_node);
		new_node = NULL;
		return NULL;
	}


	if(S_ISDIR(statbuf.st_mode))
		new_node->type = TYPE_DIR;
	else
		new_node->type = TYPE_FILE;
	
	if(*head == NULL) {
		*head = new_node;
	} else{
		now_node = *head;
		while(now_node != NULL && now_node->next != NULL)
			now_node = now_node->next;
		
		now_node->next = new_node;
	}

	return new_node;
}

void free_node_subtree(node_sub *node) {
	if(node == NULL)
		return;

	if(node->child != NULL)
		free_node_subtree(node->child);
	
	if(node->next != NULL) 
		free_node_subtree(node->next);
	memset(node->path,0,1024);
	memset(node->name,0,256);
	node->child = NULL;
	node->next = NULL;
	free(node);
}

// 로그 작성을 위한 로그 문자열 관련 함수.
void get_time_string(char *time_string) {
	time_t now_time = time(NULL); 
	struct tm *time_tm;
	char tmp[30];

	memset(tmp,0,sizeof(tmp));

	time_tm = localtime(&now_time);
	strftime(tmp, sizeof(tmp), "[%Y-%m-%d %H:%M:%S]",time_tm);
	strcpy(time_string,tmp);
}

void get_state_string(int state, char *state_string)  {
	char tmp[20];
	memset(tmp,0,sizeof(tmp));

	if(state == CREATED)
		sprintf(tmp, "%s", "[create]");
	else if(state == DELETED)
		sprintf(tmp, "%s", "[remove]");
	else if(state == MODIFIED)
		sprintf(tmp, "%s", "[modify]");

	strcpy(state_string, tmp);
}

void print_state(node_sub *monitoring_node) {
	printf("%d",monitoring_node->state);

	if(monitoring_node->child != NULL)	
		print_state(monitoring_node->child);

	if(monitoring_node->next != NULL) 
		print_state(monitoring_node->next);
}
