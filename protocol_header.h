#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <time.h>
#include <signal.h> //시그널 매크로 사용을 위한 추가
#include "cJSON.h"


#define MAX_CLIENTS_PER_ROOM 10
#define BUFFER_SIZE 10240
#define MAX_ROOMS 100000
//===========채팅 관련 프로토콜 헤더(300번대)=========================
#define CATTING_ROOM_OPEN 300 //채팅방 서브 서버 개설 요청(방 제목/비번)
#define ACK_ROOM_OPEN 301 //서버 개설 응답
#define CHATTING_LIST 302 //채팅방 목록 요청(클라이언트->서버)
#define ACK_CHATTING_LIST 303 //채팅방 목록 응답(서버->클라이언트)
#define CHATTING_ROOM_CHOOSE 304 //채팅방 선택(클라이언트->서버)
#define ACK_FAIL_ROOM_OPEN 305 // 방 생성 실패 (서버 -> 클라이언트)
#define ROOM_JOIN_REQ 306 // 방에 참가할때 (클라이언트 -> 서버)
#define ACK_ROOM_CLOSE 307 // 채팅방 종료시 클라이언트에세 알리기 (서버 -> 클라이언트)
#define CHATTING_MSG 308  // 채팅방 대화시 클라이언트가 나한테 대화내용 보내기
#define EXIT_ROOM 309     //방장이 방을 종료할시 (클라이언트 -> 서버)
#define ACK_JOIN_ROOM 310 //방에 입장 성공시 반환 (서버 -> 클라이언트)
#define ACK_EXIT_ROOM 311        // 일반 클라이언트가 나갈시 종료 (서버->클라이언트)

//===========메세지 관련 프로토콜 헤더(600번대)=========================
#define FIND_ID 600 //아이디 찾기(클라이언트->서버)
#define SUCCESS_FINE_ID 601 //아이디 있을 경우(서버->클라이언트)
#define FAIL_FINE_ID 602 //아이디 없을 경우(서버->클라이언트)
#define SEND_MSG 603 //메세지 전송(클라이언트->서버)(대상 ID 20Bytes, 내용 490Bytes)
#define MSG_LIST 604 //내 메세지 목록 요청(클라이언트-> 서버)
#define ACK_MSG_LIST 605 //저장된 메세지 항목 전송(서버->클라이언트)
#define REFRESH_MSG_LIST 606 //온라인 상에서 받은 메세지를 위해 메세지 목록 최신화 요청(클라이언트->서버)
#define ACK_REFRESH_MSG_LIST 607 //메세지 목록 최신화 응답(서버->클라이언트)
#define READ_MSG 608 //메세지 읽고 나서 서버에게 알려주기(클라이언트-> 서버)
//메세지 목록을 보여줄 때마다 자동 최신화 됌.


//===========개인설정 관련 프로토콜 헤더(900번대)=========================
#define LOGIN_START 900 //로그인 시(클라이언트->서버)
#define ACK_LOGIN_START 901 //로그인 완료 응답(서버->클라이언트)
#define MEMBER_JOIN 902 //회원가입 요청(클라이언트->서버)
#define ACK_MEMBER_JOIN 903 //회원가입 요청 응답(서버->클라이언트)
#define CHANGE_NICKNAME 904 //닉네임 변경 요청(클라이언트->서버)
#define ACK_CHANGE_NICKNAME 905 //닉네임 변경 요청 응답(서버->클라이언트)
#define PRINT_CHATTING 906 //최대 채팅 출력 갯수 수정(클라이언트->서버)
#define ACK_CHATTING 907 //최대 채팅 출력 갯수 수정 응답(서버->클라이언트)
#define PRINT_MSG 908 //최대 메세지 출력 갯수 수정(클라이언트->서버)
#define ACK_MSG 909 //최대 메세지 출력 갯수 수정 응답(서버->클라이언트)
#define DELETE_MSG 910 //이미 읽은 메세지만 삭제 요청(클라이언트-> 서버)
#define ACK_DELETE_MSG 911 //이미 읽은 메세지만 삭제 응답(서버->클라이언트)
#define ALL_MSG_DEL 912 //읽지 않은 메세지와 읽은 메세지 전부 삭제 요청(클라이언트-> 서버)
#define ACK_ALL_MSG_DEL 913 //읽지 않은 메세지와 읽은 메세지 전부 삭제  응답(서버->클라이언트)


#define BLACK "\033[0;30m"
#define RED "\033[0;31m"
#define GREEN "\033[0;32m"
#define YELLOW "\033[0;33m"
#define BLUE "\033[0;34m"
#define PURPLE "\033[0;35m"
#define CYAN "\033[0;36m"
#define END "\033[0m"
#define BOLD "\033[1m"



//유저 기본 정보 구조체
typedef struct
{
    char user_id[40];
    char password[20];
    char nickname[40];
} User;


//메세지 목록에 필요한 변수
typedef struct {

    char msg_id[20];
    char from_id[20];
    char recv_contents[490];
    char pre_contents[50];    // 목록용 (내용 일부)
    char recv_day_time[50];   // 200은 너무 크니 적절히 조절
    char read_or_not[20];     // "읽음" 또는 "안읽음"
} MessageInfo;


typedef struct 
{
    int room_id;    // 방 번호
    char room_title[50];  //방 제목
    char password[20];      // 방 비밀번호
    int host_sock;          // 방장의 소켓 (방장 판별용)
    int client_socks[MAX_CLIENTS_PER_ROOM]; // 한방에 10명 소켓 관리
    int user_count;                         // 유저가 몇명이 들어가는지 카운팅용
    pthread_mutex_t room_mutex;             // 쓰레드 관리
    bool is_active;                         // 이 방이 현재 사용 중인지 체크
} ChatRoom;


typedef struct //2중 구조체 클라이언트 개별 정보 구조체
{
    int socket;         // 방장 소켓 번호 저장
    char user_id[20];   // 방장 아이디 저장
    char nickname[20];  // 방장 닉네임 저장
    ChatRoom* current_room; // NULL이면 대기실, 아니면 해당 방에 있는 것
}ClientInfo;

