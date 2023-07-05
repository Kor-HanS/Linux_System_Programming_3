#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "ssu_monitor.h"
#include "ssu_daemon.h"

node *head = NULL; // 모니터링 중인 디몬 프로세스의 정보를 가진 구조체의 링크드 리스트 헤드. -> 모니터링중인 디몬 프로세스의 개수 만큼 있음.
const char ORDER[ORDER_SIZE][ORDERLEN] = {"add", "delete", "tree", "help", "exit"};

void ssu_monitor() {
	char temp[BUFFER_SIZE];
	char buf[BUFFER_SIZE];
	char *token = NULL;
	char monitor_txt[BUFFER_SIZE];
	
	sprintf(monitor_txt,"%s/%s",getenv("PWD"),MONITOR_TXT);

	// 디몬 프로세스를 생성한 적이 있어, monitor_list.txt 가 존재한다면 리스트 읽어오기.
	if(!access(monitor_txt, F_OK))
		read_daemon(&head);

	while(1) {
		printf("20192392> ");
		if (fgets(buf, BUFFER_SIZE, stdin) < 0) {
			fprintf(stderr, "fget error\n");
			exit(1);
		}

		if(buf[0] == '\n') // 프롬프트 상에서 엔터만 입력 시 프롬프트 재 출력.
			continue;

		if(buf[strlen(buf)-1] == '\n') // 개행 문자 제거. 
			buf[strlen(buf)-1] = '\0';
		
		strcpy(temp, buf);
		token = strtok(temp, " ");
		if(!strcmp(token,ORDER[4])) // exit 명령어 처리.
			break;
		
		do_order(buf); // 명령어 수행하기.
	}
}

void do_order(char *buf) {
	int i,argc = 0;
	char *argv[TOKEN_SIZE];
	char *token = NULL;

	for(i = 0 ; i < TOKEN_SIZE; i++)
		argv[i] = NULL;
	
	memset(argv, 0,sizeof(argv));
	token = strtok(buf, " "); // strtok() 함수를 통해 ssu_monitor() 에서의 사용자의 입력을 토큰화 합니다.
	while(token != NULL) {
		argv[argc] = (char *)malloc(sizeof(char) * BUFFER_SIZE); 
		strcpy(argv[argc++],token);
		token = strtok(NULL, " ");
	}
	
	// 토큰화 해서 첫 토큰이 내장 명령어 3개(add,delete,tree)중 하나와 일치한다면 TRUE 반환.
	// help 명령어 또는 정의 되지 않은 명령어에 대해선 print_usage()를 통해 usage 출력 후, 프롬프트 재 출력.
	if(!strcmp(ORDER[0], argv[0] ))
		ssu_add(argc, argv);
	else if(!strcmp(ORDER[1], argv[0]))
		ssu_delete(argc, argv);
	else if(!strcmp(ORDER[2], argv[0])) 
		ssu_tree(argc, argv);
	else 
		print_usage();	
}

void print_usage() {
	printf("Usage list of ssu_monitor\n");
	printf(" 1.add <DIRPATH> [OPTION]\n");
	printf("\t-t <TIME>: daemon process restart the monitoring with <TIME> interval\n");
	printf(" 2.delete <DAEMON_PID>\n");
	printf(" 3.tree <DIRPATH>\n");
	printf(" 4.help\n");
	printf(" 5.exit\n");
}

void free_token(int argc, char *argv[]) {
	int i;
	for(i = 0; i < argc; i++) {
		free(argv[i]);
		argv[i] = NULL;
	}
}

void ssu_add(int argc, char *argv[]){
	
	char absolute_path[BUFFER_SIZE];
	int tOption = FALSE;
	int time = 1; // default setting 
	struct stat statbuf;

	if(argc != 2 && argc != 4) { // add <DIRPATH> || add <DIRPATH> -t <TIME> 을 제외한 사용은 오류 처리.
		fprintf(stderr,"Usage : add <DIRPATH> [OPTION]\n");
		free_token(argc, argv);
		return; // 프롬프트 재 출력.
	}

	if(argc == 4) {
		if(!strcmp(argv[2], "-t")) 
			tOption = TRUE;

		if(!tOption || (time = get_time(argv[3])) < 0) { // 인자를 4개 받았지만, -t 옵션이 없거나, 정상적인 시간 입력이 아닐경우 에러 처리
			fprintf(stderr, "Invalid Use of -t Option\n");
			free_token(argc, argv);
			return; // 프롬프트 재 출력.
		}
	}

	if(!get_path(argv[1], absolute_path)) { // 첫번째 인자의 경로가 잘못된 경우.(경로 없는 경우 포함)
		fprintf(stderr,"Usage : add <DIRPATH> [OPTION]\n");
		free_token(argc, argv);
		return; // 프롬프트 재 출력.
	}

	if(lstat(absolute_path, &statbuf) < 0) {
		fprintf(stderr, "lstat error for %s\n",absolute_path);
		free_token(argc, argv);
		exit(1); // 프로그램 에러 처리 종료.
	}

	if(!S_ISDIR(statbuf.st_mode)) { // 첫번째 인자의 경로가 디렉토리가 아닐 경우.
		fprintf(stderr,"Usage : add <DIRPATH> [OPTION]\n");
		free_token(argc, argv);
		return; // 프롬프트 재 출력.
	}
	
	if(check_daemon(&head, absolute_path)) { // 해당 경로의 상위,하위, 자기 자신이 모니터링 중인지 링크드리스트에서 모니터링하는 디몬 프로세스 노드들의 모니터링하는 경로들과 비교
		fprintf(stderr, "(%s) is alraedy monitoring in daemon process\n",absolute_path);
		free_token(argc, argv);
		return; // 트리를 순회하여 해당 경로가 이미 모니터링 중임을 에러 처리 한 후, 프롬프트 재 출력.
	}
	
	init_daemon(&head, absolute_path, tOption, time);
	printf("monitoring started (%s)\n",absolute_path);
	free_token(argc, argv);
}

void ssu_delete(int argc, char *argv[]) {
	
	node *now_node = head;
	pid_t daemon_pid;
	char daemon_path[BUFFER_SIZE];
	int daemon_process_exist = FALSE;

	if(argc != 2) { 
		fprintf(stderr, "Usage : delete <DAEMON_PID>\n");
		free_token(argc, argv);
		return;
	}
	
	daemon_pid = atoi(argv[1]);

	while(now_node != NULL) {
		if(now_node->pid == daemon_pid){ 
			strcpy(daemon_path, now_node->path);
			daemon_process_exist = TRUE; // monitor_list.txt에 첫번째 인자 경로가 존재했다.
			break;
		}
		now_node = now_node->next;
	}

	if(!daemon_process_exist) {
		fprintf(stderr, "<DAEMON_PID> : (%d) is not exist in \"moniter_list.txt\"\n",daemon_pid);
		free_token(argc, argv);
		return; // 프롬프트 재 출력.
	}

	pop_node(&head, daemon_pid); // 디몬 프로세스를 관리하는 링크드 리스트에서 해당 프로세스와 관련된 노드를 삭제.
	kill(daemon_pid,SIGUSR1); // 임시로 죽이는것만 생각해서 넣음. 프로세스 죽이기.
	erase_daemon(daemon_path, daemon_pid); // 파일에 해당 디몬 프로세스 지우기.
	printf("monitoring ended (%s)\n",daemon_path);
	free_token(argc, argv);
}

void ssu_tree(int argc, char *argv[]) {
	node *now_node = NULL;
	char absolute_path[BUFFER_SIZE];
	char *temp;
	struct stat statbuf;
	int dirpath_exist = FALSE;
	char monitor_txt[BUFFER_SIZE];
	
	sprintf(monitor_txt,"%s/%s",getenv("PWD"),MONITOR_TXT);

	if(argc < 2) { // 첫번째 인자가 없는 경우.
		fprintf(stderr,"Usage : tree <DIRPATH>\n");
		free_token(argc, argv);
		return; // 프롬프트 재 출력.
	}

	if(!get_path(argv[1], absolute_path)) { // 첫번째 인자의 경로가 잘못된 경우.(경로 없는 경우 포함)
		fprintf(stderr,"Usage : tree <DIRPATH>\n");
		free_token(argc, argv);
		return; // 프롬프트 재 출력.
	}

	if(lstat(absolute_path, &statbuf) < 0) {
		fprintf(stderr, "lstat error for %s\n",absolute_path);
		free_token(argc, argv);
		exit(1); // 프로그램 에러 처리 종료.
	}

	if(!S_ISDIR(statbuf.st_mode)) { // 첫번째 인자의 경로가 디렉토리가 아닐 경우.
		fprintf(stderr,"Usage : tree <DIRPATH>\n");
		free_token(argc, argv);
		return; // 프롬프트 재 출력.
	}

	// 현재 프롬프트가 실행되는 도중에 모니터링 되던 경로에서 변동사항이 있을 수 있으므로, head에 있는 노드들을 모두 지운후. 다시 읽기.
	now_node = head;
	while(now_node != NULL) {
		free_node_subtree(now_node->child);
		now_node->child = NULL;
		head = now_node->next;
		now_node->next = NULL;
		free(now_node);
		now_node = head;
	}

	// 디몬 프로세스를 생성한 적이 있어, monitor_list.txt 가 존재한다면 리스트 읽어오기.
	if(!access(monitor_txt, F_OK))
		read_daemon(&head);
	
	now_node = head;
	
	while(now_node != NULL) {
		if(!strcmp(now_node->path, absolute_path)){ 
			dirpath_exist = TRUE; // monitor_list.txt에 첫번째 인자 경로가 존재했다.
			temp = strrchr(now_node->path, '/'); // 절대 경로에서 마지막 '/'를 기준으로 뒤에 디렉토리 이름을 temp에 저장.
			printf("%s\n",temp+1); // 현재 디렉토리의 이름을 깊이 0 출력.
			print_tree(&now_node->child, 1);  // 자식 노드들을 깊이 1로 시작하여 출력한다.
		}
		now_node = now_node->next;
	}
	
	if(!dirpath_exist) { // <DIRPATH> 가 moniter_list.txt에 존재하지 않는 경우. 에러 처리
		fprintf(stderr, "(%s) is not exist in \"moniter_list.txt\"\n",absolute_path);
		free_token(argc, argv);
		return; // 프롬프트 재 출력.
	}

	free_token(argc, argv);
}

int get_path(const char *path, char* absolute_path) {
	// 상대 경로 및 '~'가 처음에 포함된 경로를 절대 경로로 바꾸어 인자로 받은 absolute_path에 저장하고,
	// 정상적인 경로인지 판별하는 함수. TRUE(1) / FALSE(0) 반환. 
	char temp[BUFFER_SIZE];
	char *result;

	if(path[0] == '~')  // 처음에 '~' 를 포함한 경로.
		sprintf(temp,"%s%s",getenv("HOME"),path+1);
	else if(path[0] != '/') // 처음에 '/'를 넣지 않은 상대 경로.
		sprintf(temp, "%s/%s",getenv("PWD"),path);
	else 
		strcpy(temp,path); // 절대 경로일 경우.

	if((result = realpath(temp,NULL)) == NULL)
		return FALSE;
	
	strcpy(absolute_path, result);
	return TRUE;
}

int get_time(char *time)  {
	// 정상적으로 <TIME> 의 값을 리턴하면 0이상의 int형 정수 , 비정상적으로 값 리턴 -1  
	char *c = time;
	int result = 0;
	
	while(*c != '\0') {
		if(*c >= '0' && *c <= '9')
			result = result * 10 + *c - 48;
		else 
			return -1;
		c++;
	}
	return result;
}

void read_daemon(node **head) {
	// ssu_monitor() 에서 프롬프트 내장 명령어 수행 전, monitor_list.txt 가 존재하면 해당 함수를 불러 링크드리스트 생성.
	char monitor_txt[1024];
	FILE *fp; // monitor_list.txt 에 대한 파일 스트림.
	char buf[BUFFER_SIZE]; 
	char *token,node_path[BUFFER_SIZE],node_pid_str[10];
	pid_t node_pid;
	sprintf(monitor_txt,"%s/%s",getenv("PWD"),MONITOR_TXT); // 디몬 프로세스에게 monitor_list.txt 의 절대 경로를 줌으로써, 사용자가 프로그램을 실행한 곳에 모니터 리스트 txt 를 생성.

	if((fp = fopen(monitor_txt, "rb")) == NULL) {
		fprintf(stderr, "fopen error for %s\n",monitor_txt);
		exit(1);
	}

	while(fgets(buf, BUFFER_SIZE, fp) != NULL) {
		buf[strlen(buf)-1] = '\0';
		token = strtok(buf, " ");
		strcpy(node_path, token);
		token = strtok(NULL, " ");
		strcpy(node_pid_str, token);
		node_pid = atoi(node_pid_str);
		push_back_node(head, node_path, node_pid);
	}

	fclose(fp);
}

void erase_daemon(const char *path, pid_t pid) {
	// pid 와 관리하는 경로가 일치하는 디몬 프로세스를 파일에서 지운다.
	// 임시로 monitor_list_tmp.txt 를 만들고, 해당하는 프로세스 외의 프로세는 쓴 후, 파일의 이름을 변경한다.
	FILE *fp_original;
	int fd_tmp;
	char monitor_txt[1024]; 
	char monitor_tmp_txt[1024];
	char buf[BUFFER_SIZE];
	
	char monitor_path[1024],monitor_pid[1024];
	char *token;

	sprintf(monitor_txt,"%s/%s",getenv("PWD"),MONITOR_TXT);
	sprintf(monitor_tmp_txt, "%s/%s",getenv("PWD"),"monitor_list_tmp.txt");

	if((fp_original = fopen(monitor_txt, "rb")) == NULL) {
		fprintf(stderr, "erase_daemon : fopen error for %s\n",monitor_txt);
		exit(1);
	}

	if((fd_tmp = open(monitor_tmp_txt, O_WRONLY | O_CREAT | O_TRUNC, 0664)) < 0) {
		fprintf(stderr, "erase_daemon : open error for %s\n",monitor_tmp_txt);
		exit(1);
	}

	while(fgets(buf, BUFFER_SIZE, fp_original) != NULL) {
		buf[strlen(buf)-1] = '\0';
		token = strtok(buf, " ");
		strcpy(monitor_path, token);
		token = strtok(NULL, " ");
		strcpy(monitor_pid, token);

		if(!strcmp(path, monitor_path) && atoi(monitor_pid) == pid) {
			continue;
		} else {
			sprintf(buf,"%s %s\n",monitor_path,monitor_pid);
			if(write(fd_tmp, buf, strlen(buf)) < 0) {
				fprintf(stderr, "write error for %s\n",monitor_txt);
			}
		}
	}
	
	// 임시 파일을 작성한 후, 원본을 지우고 임시 파일 명을 monitor_list.txt로 변경한다.
	if(unlink(monitor_txt) < 0) {
		fprintf(stderr, "erase_daemon : unlink error for %s\n",monitor_txt);
		exit(1);
	}
	
	if(rename("monitor_list_tmp.txt", MONITOR_TXT) < 0 ) {
		fprintf(stderr, "erase_daemon : rename error for %s\n",monitor_tmp_txt);
		exit(1);
	}

	fclose(fp_original);
	close(fd_tmp);
}

void init_daemon(node **head, char *path, int tOption, int time) {
	pid_t pid;
	node *daemon_node_old = NULL; // 디몬 프로세스 내에서 쓸 node* 비교를 위해 get_node() 를 통해 해당 디몬 프로세스가 관리하는 노드를 모니터링 재시작 마다 만든다.
	node *daemon_node_new = NULL;
	int fd;// monitor_list.txt 에 대한 파일 디스크립터.
	int maxfd; // 열려있는 파일 디스크립터의 수.
	char buf[BUFFER_SIZE];
	char monitor_txt[4096];
	struct sigaction del_act; 
	
	sprintf(monitor_txt, "%s/%s",getenv("PWD"),MONITOR_TXT);

	if((pid = fork()) < 0) {
		fprintf(stderr, "fork error\n");
		exit(1);
	} else if(pid != 0) { 
		if((fd = open(monitor_txt, O_WRONLY | O_CREAT, 0664)) < 0) {
			fprintf(stderr, "make_daemon : open error\n");
			exit(1);
		}

		sprintf(buf,"%s %d\n",path,pid);
		lseek(fd,(off_t)0, SEEK_END);
		if(write(fd, buf, strlen(buf)) < 0) {
			fprintf(stderr, "make_daemon : write error\n");
			exit(1);
		}
		close(fd);
		push_back_node(head,path,pid);
		return;
	}
	
	setsid(); // 독자적 세션 생성.
	signal(SIGTTIN, SIG_IGN); // 제어 터미널 관련 시그널 제거.
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTSTP, SIG_IGN); 
	
	maxfd = getdtablesize(); // 열려있는 파일 디스크립터 모두 닫기.
	for(fd = 0; fd < maxfd; fd++)
		close(fd);

	umask(0); // 파일의 마스크 금지.
	chdir("/"); // 디렉토리 루트로 변경(옵션).
	
	fd = open("/dev/null",O_RDWR); // 제어 터미널 없음.
	dup(0);
	dup(0);

	pid = getpid();
	daemon_node_old = get_node(path, pid);

	del_act.sa_flags = 0;
	sigemptyset(&del_act.sa_mask);
	del_act.sa_handler = ssu_signal_handler;
	
	if(sigaction(SIGUSR1, &del_act, NULL) != 0){
		fprintf(stderr, "sigaction error\n");
		exit(1);
	}

	while(1) { 
		if(tOption)
			sleep(time); // -t <TIME> 프롬프트 add 명령어 실행전, add 명령어 내부에서 tOption이 있다면 옵션 사용이 올바른지 검사한다.
		else 
			sleep(1);
	
		// 디몬 프로세스가 파일의 대해 모니터링함.
		daemon_node_new = get_node(path, pid);

		// 새로운 노드와 예전 노드의 트리를 비교하여 달라진 점을 로그에 저장한다.
		compare_node(daemon_node_old, daemon_node_new);
		
		// 오래된 노드를 반환하고, 새로운 노드를 현재 관리 중인 노드로 바꾼다.
		free_node_subtree(daemon_node_old->child);
		daemon_node_old->child = NULL;
		daemon_node_old->next = NULL;
		free(daemon_node_old);
		daemon_node_old = NULL;
		daemon_node_old = daemon_node_new;
		daemon_node_new = NULL;
		initialize_nodes(daemon_node_old);
	}
	exit(0);
}

int check_daemon(node **head, const char *path) { // 데몬 프로세스가 관리하는 프로세스에 해당 경로가 있는지 확인.
	node *now_node = *head;

	while(now_node != NULL) {
		// 비교 하는 두 스트링 중 길이가 더 짧은 쪽의 것을 기준으로 해당 길이에 해당하는 부분이 같은지 비교.
		// 만약 path가 상위 경로라면  path가 더 길이가 짧고, path 만큼의 부분이 같음.
		// 만약 둘이 같은 경로라면 같은 길이 path 만큼의 부분이 같음.
		// 만약 path가 하위 경로라면 now_node->path가 더 길이가 짧고, 해당 노드의 경로 길이만큼 부분이 같음.
		if(strlen(path) < strlen(now_node->path) && !strncmp(path, now_node->path,strlen(path))) {
			if(path[0] == '/' && strlen(path) == 1) //  절대 경로 : '/' 입력에 대한 예외 처리.
				return TRUE;
			else if(now_node->path[strlen(path)] == '/')  // 경로가 겹쳐야됨(상위 경로 입력). 같은 경로상에서 이름이 겹치는 경우 FALSE ex) /home/test , /home/test2
				return TRUE;
		} else if(strlen(path) == strlen(now_node->path) && !strcmp(path, now_node->path)){
			return TRUE;
		} else if(strlen(path) > strlen(now_node->path) && !strncmp(path, now_node->path,strlen(now_node->path))) {
			if(path[strlen(now_node->path)] == '/')  // 경로가 겹쳐야됨(하위 경로 입력). 같은 경로상에서 이름이 겹치는 경우 FALSE ex) /home/test , /home/test2
				return TRUE; 
		}
		now_node = now_node->next;
	}
	return FALSE;
}

void push_back_node(node **head, char *path, pid_t pid) {
	node *now_node = NULL;
	node *new_node = (node*)malloc(sizeof(node));
	DIR *dir;

	memset(new_node->path, 0, sizeof(new_node->path));
	strcpy(new_node->path,path);
	new_node->pid = pid;
	new_node->next = NULL;
	new_node->child = NULL;
	
	if((dir = opendir(path)) == NULL) {
		fprintf(stderr, "push_back_node() : opendir error for %s\n",path);
		free(new_node);
		exit(1);
	}

	closedir(dir);

	make_tree(&new_node->child,path); // 디몬 프로세스가 모니터링 하는 node의 하위 서브디렉토리 및 파일을 위한 링크드리스트 트리를 만든다. 
	
	if(*head == NULL) {
		*head = new_node;
	} else{
		now_node = *head;
		while(now_node-> next != NULL)
			now_node = now_node->next;
	
		now_node->next = new_node;
	}
}

void pop_node(node **head, pid_t pid) { 
	node *prev_node = NULL;
	node *now_node = *head;
	while(now_node != NULL) {
		if(now_node->pid == pid) {
			if(prev_node != NULL) {
				prev_node->next = now_node->next; // prev_node 가 있다는 것은 현재 노드의 이전 노드의 다음을 현재 노드의 다음으로 만든다.
				
				free_node_subtree(now_node->child);
				now_node->child = NULL;
				now_node->next = NULL;
				free(now_node);
				now_node = NULL;

			} else {
				*head = now_node->next;

				free_node_subtree(now_node->child);
				now_node->child = NULL;
				now_node->next = NULL;
				free(now_node);
				now_node = NULL;
			}
			return;
		}
		prev_node = now_node; // 이전 노드를 현재 노드로 만들고,
		now_node = now_node->next; // 현재 노드를 다음 노드로 이동시킨다.
	}
}

void print_node(node **head) {
	node *now_node = *head;
	while(now_node != NULL) {
		printf("%s %d\n",now_node->path, now_node->pid);
		now_node = now_node->next;
	}
}

void ssu_signal_handler(int signo) {
	if(signo == SIGUSR1) {
		exit(0);
	} else {
		fprintf(stderr, "unexpected signal\n");
		exit(1);
	}
}
