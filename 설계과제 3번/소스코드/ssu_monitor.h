#ifndef TRUE
	#define TRUE 1
#endif

#ifndef FALSE
	#define FALSE 0
#endif

#define  ORDERLEN 10

#define BUFFER_SIZE 4096
#define TOKEN_SIZE 10 // char *argv[] 의 최대 개수.
#define ORDER_SIZE 5 // add, delete, tree, help, exit

// 디몬 프로세스가 모니터링 중인 경로, pid의 정보를 가진 노드.
typedef struct node{ 
	char path[1024];
	pid_t pid;
	struct node *next;
	struct node_sub *child; // 현재 디몬 프로세스가 관리하는 경로 하위 파일 트리의 헤드.
}node;

void ssu_monitor(); // 사용자의 입력을 처리 add,delete,tree -> do_order(), exit, help
void do_order(char *buf); // 내장 명령어 수행.
void print_usage(); // 내장 명령어 help를 위한 usage 출력.

void free_token(int argc, char *argv[]); // 내장 명령어 수행을 위해 만든 argv 토큰 명령어를 free() 한다.
void ssu_add(int argc, char *argv[]); // 내장 명렬어 add 
void ssu_delete(int argc, char *argv[]); // 내장 명령어 delete
void ssu_tree(int argc, char *argv[]); // 내장 명령어 tree

// 유틸리티 함수.
int get_path(const char *path, char *absolute_path); // 적절한 경로인지 검사 및 absolute_path 에 절대 경로로 변환한 경로를 저장하는 함수.
int get_time(char *time); // add 명령어 도중 -t <TIME> 옵션 처리를 위한 함수, <TIME> 에 해당하는 인자가 정상적인 정수형 수를 리턴한다. 비정상적인 인자 입력시, -1 리턴.

// 내부 명령어 수행을 위한 함수.
void read_daemon(node **head); // monitor_list.txt를 통해 모든 디몬 프로세스를 노드로 가진다. -> 프로그램 시작시, 어떤 디몬 프로세스가 동작하고 있는지 링크드 리스트로 관리 가능.
void erase_daemon(const char *path, pid_t pid); // monitor_list.txt 에서 해당 디몬 프로세스 삭제
void init_daemon(node **head, char *path, int tOption, int time); // 디몬 프로세스 생성 함수 -> 해당 경로에 대한 모니터링 시작.
int check_daemon(node** head, const char *path); // 해당 경로에 대한 디몬 프로세스가 존재하는지 확인 하는 함수 TRUE 면 있음. FALSE 면 없음.

// 디몬 프로세스가 관리하는 링크드 리스트. node 내에 디몬 프로세스가 관리하는 경로와 디몬 프로세스 pid 관리.
void push_back_node(node **head, char *path, pid_t pid); // 경로와 디몬 프로세스의 pid를 통해 링크드 리스트에 노드 추가.
void pop_node(node **head, pid_t pid); // pid에 해당하는 노드 링크드 리스트에서 지우기.
void print_node(node **head);

// delete 내부 명령어를 통해 디몬 프로세스의 SIGUSR1 시그널을 보낼때
// 디몬 프로세스 내부에서 SIGUSR1의 핸들러 함수로 등록하고 수행한다.
void ssu_signal_handler(int signo); 
