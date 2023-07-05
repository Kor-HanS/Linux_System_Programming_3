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

int main(void) {
	ssu_monitor(); // 내부 명령어 실행을 위한 모니터링 프롬프트 함수 실행.
	exit(0);
}
