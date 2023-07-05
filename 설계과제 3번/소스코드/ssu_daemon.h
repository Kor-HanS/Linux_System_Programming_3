#ifndef MONITOR_TXT
	#define MONITOR_TXT "monitor_list.txt"
#endif

#ifndef LOG_TXT
	#define LOG_TXT "log.txt"
#endif

#ifndef ERR_TXT
	#define ERR_TXT "err.txt"
#endif

#ifndef TYPE_FILE
	#define TYPE_FILE 0
#endif

#ifndef TYPE_DIR 
	#define TYPE_DIR 1
#endif

#ifndef STATES  // node_sub 가 갖는 상태.
	#define UNCHECKED 0
	#define CHECKED 1
	#define CREATED 2 
	#define DELETED 3
	#define MODIFIED 4
#endif

typedef struct node_sub{ // 현재 디몬 프로세스가 모니터링 중인 경로의 하위 파일 및 서브 디렉토리의 경로.
	int type; // 파일이면 0 서브 디렉토리면 1
	int state; // UNCHECKED ~ MODIFIED 0 ~ 4 번 상태. 2,3,4의 경우 로그 파일의 작성.
	char path[1024]; // node의 경로(디렉토리 경로, 파일 경로)
	char name[256]; // node의 이름.(디렉토리 명, 파일 명)
	time_t node_mtime;
	struct node_sub *next; // 현재 자신과 같은 레벨의 파일 이나 서브 디렉토리 node_sub
	struct node_sub *child; // 현재 자신이 디렉토리 타입이라면 NULL이 아닐수 있음. ".", ".." 제외
}node_sub;

node* get_node(const char *path, pid_t pid); // 다른 노드 함수와 달리, 디몬 프로세스내에서 현재 관리하는 경로와 pid 를 통해 노드를 생성하는 함수, 만약 관리하는 경로 디렉토리 자체가 사라진다면, 해당  로그를 통해 디몬 프로세스가 관리하는 디렉토리가 삭제되었음을 찍고, 디몬 프로세스를 종료시킨다. -> 아무것도 없는 경로 모니터링 해제.

// node_sub의 state를 초기화 하기 위한 함수.
void initialize_nodes(node *monitoring_node); // 디몬 프로세스가 모니터링 중인 노드의 하위 파일/서브 디렉토리 노드들의 state 를 UNCHECKED로 초기화한다.
void initialize_node(node_sub *now_node);

// 디몬 프로세스 내부 함수.
void compare_node(node *old_node, node *new_node); // get_node() 를 통해 현재 관리 되고 있는 트리 링크드리스트가 변경사항이 있는가 비교하는 함수.
void compare_node_sub(int fd, node_sub *old_node_sub, node_sub *new_node_sub); // 두 node_sub 트리를 인자로 받고, new_node_sub 트리를 순회하며, 파일 타입 일 경우. compare_info() 호출e
void compare_info(node_sub *old_node, node_sub *new_node); // old_node_sub 트리를 순회하며, new_node를 찾고, 정보를 비교하여, state를 초기화한다.
void check_node_deleted(int fd, node_sub *old_node); // old_node_sub 트리를 순회하여, new_node에는 찾을수 없는 UNCHEKCED를 유지하는 노드를 삭제 처리 한다.

// node_sub 관련 함수. -> node_sub 트리는 node 하위의 파일 트리 입니다.
void make_tree(node_sub **head, const char *path); // 해당 경로의 하위 파일 및 서브 디렉토리의 트리를 만든다.
void print_tree(node_sub **head, int depth); // 해당 경로의 하위 파일 및 서브 디렉토리의 트리를 출력한다.

node_sub *push_back_node_sub(node_sub **head, char *path, char *name); // node_sub 삽입 후 해당 노드 반환.
void free_node_subtree(node_sub *node); 

// 로그 작성을 위한 time 과 변경 사항 상태 문자열.
void get_time_string(char *time_string);
void get_state_string(int state, char *state_string);

// node 하위 node_sub 파일 트리의 state를 확인하기 위한 함수.
void print_state(node_sub *monitoring_node);
