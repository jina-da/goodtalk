#include "protocol_header.h"

#define BUFFER_SIZE 10240

// 최대 40개를 담을 배열 선언
MessageInfo msg_list[50];

void gotoxy(int x, int y);                          // 좌표 이동 함수 
void enter();                                       //엔터 입력받기 함수
void nickname_out(char *target_nick, int size);     //닉네임 변경 및 예외처리 함수
bool both_alphabet_len(char* word);                 // 로그인 예외처리
int input_c_out(char *contexts);                    //"/c"입력 시 입력 취소 할 수 있게 하는 함수
void render_chat_view();                            // 좌표 이동 함수 
void push_chat_history(char *msg);                  // 새로운 메시지를 배열에 추가하는 함수
void member_ship(int sock);                         // 클라이언트에서 전송하는 부분 예시
int login_user(int sock);                           // 로그인 로직
void user_setting(int sock);                        // 개인 설정 프로토콜
void message_menu(int sock);                        // 메세지 프로토콜
void chat_menu(int sock);                           // 채팅 프로토콜
void *recv_room_msg(void *arg);                     // 남이 보낸 메시지를 실시간으로 출력하는 스레드 함수
void in_room_chat(int sock);                        // 실제 채팅창 화면 로직
void error_handling(char *message);                 // 에러 처리 함수



//아이디를 계속 보내서 확인시켜줘야하니까 전역변수로 만들어두고 테스트하기
//뭐 수정할때마다 아이디 같이 담아서 보내기
//아이디와 닉네임은 계속 출력되어야해서 전역변수로 만들기(로그인 시 저장됨)
char client_id[20];
char client_nickname[25];

// 채팅 중 서버 메시지를 받는 스레드 변수
int stop_chat_flag = 0; 

// 채팅방 최대 갯수를 위한 함수
int chat_display_limit = 10;            // 기본 출력 개수 (설정에서 변경 가능)
char chat_history[30][512];             // 최근 메시지 저장용 배열
int history_cnt = 0;                    // 현재 쌓인 메시지 개수

// 좌표 이동 함수 
void gotoxy(int x, int y) {
    printf("\033[%d;%df", y, x);
    fflush(stdout);
}

//엔터치면 넘어가는 코드가 자주쓰여서 함수화
void enter() {

    printf("\n\n엔터를 입력해주세요 ...");
    while (getchar() != '\n') continue;
}


// 인자로 받은 nick_name(주소)에 직접 값을 써주는 방식입니다.(한글도 가능?)
void nickname_out(char *target_nick, int size) {

    while(1) 
    {
        char temp[100]; // 입력을 넉넉하게 받을 임시 버퍼
        int is_invalid = 0;

        printf("\n\n닉네임을 입력하세요(설정 안 하려면 /c 입력) >>>   ");

        // 1. 입력 받기
        if (fgets(temp, sizeof(temp), stdin) == NULL) continue;
        temp[strcspn(temp, "\n")] = 0; // 엔터 제거


        // 2. "/c" 입력 시 처리
        // 입력값 중에 /c가 "어디라도" 들어있으면 취소로 인식
        if(input_c_out(temp)) {
            printf("\n\n닉네임은 빈칸으로 두겠습니다. ");
            strcpy(target_nick, "");
            getchar();
            break;
        }  

        // 3. 글자 수(바이트) 체크
        if (strlen(temp) == 0) {
            printf("\n\n%s[오류] 최소 한 글자 이상 입력해주세요!%s\n", RED, END);
            getchar();
            continue;
        }
        if (strlen(temp) >= size) {
            printf("\n\n%s[오류] 너무 길어서 저장할 수 없습니다! (최대 %d바이트)%s\n", RED, size - 1, END);
            getchar();
            continue;
        }

        strcpy(target_nick, temp); // 검증된 temp를 실제 목적지인 target_nick에 복사
        return; 
    }
}


// 아이디/비번 영문자+숫자 5~15글자만 받게하기
bool both_alphabet_len(char* word) 
{
    int len = strlen(word);

    if (len < 5 || len > 15)
    {
        return false; // 길이 조건이 안맞으면 false 를 출력
    }

    for (int i = 0; i < len; i++) // 아이디를 입력받는 for문으로 하나씩 꺼내서 검수
    {

        if (!isalnum(word[i])) // isalnum: 영문자 혹은 숫자인지 확인하는 함수
        {
            return false; // 영문/숫자가 아닌 문자가 발견되면 false
        }
    }
    return true; // 모든 문자가 영문/숫자임
}


//"/c" 를 중간에 입력하면 입력 취소
int input_c_out(char* contexts) {

    if(strstr(contexts, "/c") != NULL) { 
        printf("\n\n'/c'가 감지되었습니다. 입력을 취소합니다 !\n");
        // strcpy(contexts, ""); // 결과를 빈 문자열로 설정
        return 1;
    }
    else return 0;
}

// 채팅방 최대 출력 갯수를 위한 함수
void render_chat_view() {
    system("clear");
    printf("========= [ GOOD Talk 채팅방 ] =========\n");
    
    // 1. 아래서부터 차오르게 만드는 패딩(Padding) 로직
    // 메시지가 3개고 제한이 10개라면, 7줄의 공백을 먼저 출력합니다.
    int padding = chat_display_limit - history_cnt;
    for (int i = 0; i < padding; i++) {
        printf("\n");
    }

    // 2. 저장된 메시지 출력 (gotoxy 대신 순차 출력으로 아래 정렬 효과)
    for (int i = 0; i < history_cnt; i++) {
        printf("%s\n", chat_history[i]);
    }

    // 3. 입력창 위치 고정 (지훈님이 만드신 알고리즘 적용)
    printf("========================================\n");
    gotoxy(0, chat_display_limit + 3); // 입력창 위치를 출력 개수에 맞춰 가변적으로 조절
    printf("[채팅 입력 (나가기: /q)] >> ");
    fflush(stdout);
}


// 새로운 메시지를 배열에 추가하는 함수
void push_chat_history(char *msg) {
    if (history_cnt < chat_display_limit) {
        strcpy(chat_history[history_cnt++], msg);
    } else {
        // 배열을 한 칸씩 위로 밀고 마지막에 추가 (스크롤 효과)
        for (int i = 0; i < chat_display_limit - 1; i++) {
            strcpy(chat_history[i], chat_history[i + 1]);
        }
        strcpy(chat_history[chat_display_limit - 1], msg);
    }
}


//회원가입 함수(전송가능)(전송 시 프로토콜 헤더 + 아이디 + 비번 + 닉네임 다 담기)
void member_ship(int sock)
{
    //통신을 위한 변수
    char result[20];    //성공 여부
    char name_msg[BUFFER_SIZE]; // 수신 버퍼

    //회원가입 후 보낼 떄(아이디, 비번, 닉네임 담아서 보내기)
    char make_id[20];       //id는 최대 20Bytes
    char make_pw[20];       //pw는 최대 20Bytes
    char make_nickname[25]; //닉네임 최대 25Bytes
    
    //아이디 입력 받고 예외처리
    while (1)
    {
        system("clear");
        printf("\n\n아이디를 입력해주세요(영문자 + 숫자 / 5~15자) >>>   ");

        fgets(make_id, sizeof(make_id), stdin);
        make_id[strcspn(make_id, "\n")] = 0;

        if (strlen(make_id) == 0) // 빈칸 입력 못하게 막기
        {
            printf("\n\n아이디를 입력해주세요.\n");
            continue;
        }
        else if (!both_alphabet_len(make_id))   //영숫자 + 길이계산
        {
            printf("\n\n\n❌   아이디는 영문자와 숫자만 포함할 수 있으며, 5~15자 이내여야 합니다. ");
            getchar();
            continue;
        }

        //비밀번호 입력 받고 예외처리
        while(1) 
        {
            printf("\n\n비밀번호를 입력하세요 >>>   ");

            fgets(make_pw, sizeof(make_pw), stdin);
            make_pw[strcspn(make_pw, "\n")] = 0;

            
            if (strlen(make_pw) == 0) // 빈칸 입력 못하게 막기
            {
                printf("\n\n비밀번호를 입력해주세요.\n");
                continue;
            }
            else if (!both_alphabet_len(make_pw))   //영숫자만 + 길이계산
            {
                printf("\n\n\n❌   비밀번호는 영문자와 숫자만 포함할 수 있으며, 5~15자 이내여야 합니다.");
                getchar();
                continue;
            }
            break;
        }

        //비밀번호 설정 안하고 싶을 수도 있으니깐 문자열로 받기
        char temp_make_nickname[25];
        printf("닉네임을 정해주세요 (설정 X : 엔터) >>> ");
        fgets(temp_make_nickname, sizeof(temp_make_nickname), stdin);
        temp_make_nickname[strcspn(temp_make_nickname, "\n")] = 0;

        if (strlen(temp_make_nickname) == 0) strcpy(make_nickname, ""); // 비밀번호 없음
        else strcpy(make_nickname, temp_make_nickname);


        // //닉네임 입력받고 예외처리하는 함수
        // nickname_out(make_nickname, sizeof(make_nickname));

        //확인용
        printf("\n\n서버로 요청 보낼 아이디(%s), 비번(%s), 닉네임(%s)\n\n", make_id, make_pw, make_nickname);

        cJSON *root = cJSON_CreateObject();  // 루트 객체 생성
        cJSON_AddNumberToObject(root, "protocol", MEMBER_JOIN); // 프로토콜 번호 추가

        cJSON *data = cJSON_CreateObject(); // 데이터 객체 생성
        cJSON_AddStringToObject(data, "user_id", make_id);
        cJSON_AddStringToObject(data, "password", make_pw); 
        cJSON_AddStringToObject(data, "nickname", make_nickname);

        cJSON_AddItemToObject(root, "data", data);// 데이터 객체를 루트에 추가

        char *json_string = cJSON_PrintUnformatted(root); // 한 줄 문자열로 변환
        char send_buf[BUFFER_SIZE];
        sprintf(send_buf, "%s\n", json_string); // ✅ \n 추가
        send(sock, send_buf, strlen(send_buf), 0); // 서버로 쏴라!

        free(json_string);
        cJSON_Delete(root);


        //서버의 응답 가져오기
        int str_len = recv(sock, name_msg, BUFFER_SIZE - 1, 0); // 서버로부터 메시지 수신
        if (str_len <= 0) {
            printf("\n\n수신 오류 .. 종료합니다 .. ");
            return; // 수신 오류 시 종료
        }
        name_msg[str_len] = '\0';  // 문자열 종료 문자 추가

        cJSON *response = cJSON_Parse(name_msg);
        cJSON *res_data = cJSON_GetObjectItem(response, "data");

        strcpy(result, cJSON_GetObjectItem(res_data, "result")->valuestring);   //성공여부

        if(strcmp(result, "성공") == 0) {
            printf("\n\n✨   회원가입에 성공하셨습니다 ^.^  ✨");
            enter();
            break;
        }
        else if(strcmp(result, "실패") == 0) {
            printf("\n\n❌   중복된 아이디 입니다 ! 다시 입력해주세요 !");
            enter();
            continue;
        }
    }
}

//로그인 함수(아이디, 비번 전송 => 성공 시 아이디, 닉네임 받음)
int login_user(int sock)
{
    //통신을 위한 변수
    char result[20];    //성공 여부
    char login_msg[BUFFER_SIZE]; // 수신 버퍼

    char id_input[20]; // 사용자에게 입력받을 아이디와 비밀번호 변수 선언
    char password_input[20];

    while(1) {
        system("clear");

        printf("\n\n아이디를 입력하세요(/c 입력 시 입력 취소) >>>   ");
        if (fgets(id_input, sizeof(id_input), stdin)) { // 입력받기
            id_input[strcspn(id_input, "\n")] = 0;
        }

        if(input_c_out(id_input)) {
            return 1;
        }

        printf("\n\n비밀번호를 입력하세요(/c 입력 시 입력 취소) >>>   ");
        if (fgets(password_input, sizeof(password_input), stdin)) { // 비밀번호 입력받기
            password_input[strcspn(password_input, "\n")] = 0;
        }

        if(input_c_out(password_input)) {
            return 1;
        }
        //서버로 보내기
        cJSON *root = cJSON_CreateObject();  // 루트 객체 생성
        cJSON_AddNumberToObject(root, "protocol", LOGIN_START); // 프로토콜 번호 추가

        cJSON *data = cJSON_CreateObject(); // 데이터 객체 생성
        cJSON_AddStringToObject(data, "user_id", id_input);
        cJSON_AddStringToObject(data, "password", password_input);

        cJSON_AddItemToObject(root, "data", data);// 데이터 객체를 루트에 추가

        char *login_string = cJSON_PrintUnformatted(root);
        char send_buf[BUFFER_SIZE];
        sprintf(send_buf, "%s\n", login_string); // 끝에 \n을 붙여서 패킷 구분
        send(sock, send_buf, strlen(send_buf), 0); 
        free(login_string);
        cJSON_Delete(root);


        //서버의 응답 가져오기
        int str_len = recv(sock, login_msg, BUFFER_SIZE - 1, 0); // 서버로부터 메시지 수신
        if (str_len <= 0) {
            printf("\n\n수신 오류 .. 종료합니다 .. ");
            continue;
        }
        login_msg[str_len] = '\0';  // 문자열 종료 문자 추가

        cJSON *login = cJSON_Parse(login_msg);
        cJSON *login_data = cJSON_GetObjectItem(login, "data");

        strcpy(result, cJSON_GetObjectItem(login_data, "result")->valuestring);   //성공여부
        

        if(strcmp(result, "성공") == 0) {
            
            strcpy(client_id, cJSON_GetObjectItem(login_data, "user_id")->valuestring);   //아이디 출력
            strcpy(client_nickname, cJSON_GetObjectItem(login_data, "nickname")->valuestring);   //닉네임 출력

            // ✅ 서버에서 보내준 개인 설정값을 전역 변수에 반영합니다.
            cJSON *limit_obj = cJSON_GetObjectItem(login_data, "chat_display_limit");
            if(limit_obj) chat_display_limit = limit_obj->valueint;

            printf("\n\n✨   로그인 성공! 환영합니다, %s님 !  ✨\n", client_nickname); // 로그인 성공 메시지
            enter();
            return 0;
        }
        else if(strcmp(result, "실패") == 0) {
            printf("\n\n❌   아이디 또는 비밀번호가 틀렸습니다. 다시 입력해주세요 !"); // 로그인 메시지
            enter();
            continue;
        }
    }
}

//개인설정 함수
void user_setting(int sock)  
{
    char result[20];    //수신 성공 여부
    char settings_msg[BUFFER_SIZE]; // 수신 버퍼

    char set_nickname[25]; //닉네임 담을 상자
    int set_chat_len; //채팅 길이 조절 담을 상자
    int settings_choice; //나가기 보내기 목록 확인을 입력받기 위한 변수
    int change_chat_max; //채팅창 최대 출력 개수 조절을 위한 변수

    int change_msg_del; //읽은 메세지만 삭제 or 전체 메세지를 삭제할지 선택의 입력을 담는 변수

    while(1) {

        system("clear");
        printf("\n\n%s📍    개인 설정   📍%s\n", BOLD, END);
        printf("\n\n0. 나가기   1. 닉네임 변경  2. 채팅방 설정  3. 메세지 설정\n");
        printf("\n입력 >>>   ");
        if (scanf("%d", &settings_choice) != 1) {
            printf("\n\n❌ 숫자로 입력해주세요!\n");
            while (getchar() != '\n'); // 잘못된 문자열 버리기
            enter();
            continue;
        }
        while (getchar() != '\n');

        //예외처리) 양의 정수가 아니면 모두 잘못 입력한것임
        if (settings_choice >= 4 || settings_choice < 0){
            printf("\n\n❌   잘못 입력하셨습니다.\n");
            enter();
            continue;
        }
      
        else if(settings_choice == 0){ 
            printf("\n\n0. 나가기를 선택하셨습니다.\n");
            enter();
            break;
        }

        //1. 닉네임 설정 선택
        else if(settings_choice == 1) {

            printf("\n\n1. 닉네임 변경을 선택했습니다.\n");
            enter();
            system("clear");
            nickname_out(set_nickname, sizeof(set_nickname));

            //서버에 닉네임 변경 요청
            cJSON *root = cJSON_CreateObject();
            cJSON_AddNumberToObject(root, "protocol", CHANGE_NICKNAME); 
            
            //변경될 닉네임 전송(아이디도 항상 같이 보내기)
            cJSON *data = cJSON_CreateObject();
            cJSON_AddStringToObject(data, "user_id", client_id);  //id 보내기
            cJSON_AddStringToObject(data, "nickname",set_nickname);  //변경할 닉네임 보내기
            cJSON_AddItemToObject(root, "data", data);// 데이터 객체를 루트에 추가

            char *packet = cJSON_PrintUnformatted(root);
            char send_buf[BUFFER_SIZE];
            sprintf(send_buf, "%s\n", packet); // 끝에 \n을 붙여서 패킷 구분
            send(sock, send_buf, strlen(send_buf), 0); 
            free(packet);
            cJSON_Delete(root);


            // 서버 응답을 '\n'이 나올 때까지 반복해서 받기
            memset(settings_msg, 0, BUFFER_SIZE);
            int total_len = 0;
            while (1) {
                int str_len = recv(sock, &settings_msg[total_len], BUFFER_SIZE - total_len - 1, 0);
                if (str_len <= 0) {
                    printf("\n\n%s[오류]: 서버 연결 끊김%s\n", RED, END);
                    return; // 함수 종료 혹은 적절한 처리
                }
                total_len += str_len;
                settings_msg[total_len] = '\0';

                // 패킷의 끝(\n)을 확인
                if (settings_msg[total_len - 1] == '\n') {
                    settings_msg[total_len - 1] = '\0'; // JSON 파싱을 위해 \n 제거
                    break;
                }
                if (total_len >= BUFFER_SIZE - 1) break; // 버퍼 꽉 참 방지
            }

            // [파싱] 온전해진 JSON 문자열 파싱
            cJSON *recv_root = cJSON_Parse(settings_msg);

            if (recv_root != NULL) {
                cJSON *recv_data = cJSON_GetObjectItem(recv_root, "data");
                if (recv_data) {
                    char *res_str = cJSON_GetObjectItem(recv_data, "result")->valuestring;

                    if(strcmp(res_str, "성공") == 0) {
                        char *new_nick = cJSON_GetObjectItem(recv_data, "nickname")->valuestring;
                        strncpy(client_nickname, new_nick, sizeof(client_nickname) - 1);
                        client_nickname[sizeof(client_nickname) - 1] = '\0';

                        printf("\n\n🎉   닉네임 변경에 성공하셨습니다 !");
                        printf("\n\n✨   바뀐 닉네임은 [%s%s%s] 입니다.", CYAN, client_nickname, END);
                    } else {
                        printf("\n\n%s❌   닉네임 변경 실패: %s%s\n", RED, res_str, END);
                    }
                }
                cJSON_Delete(recv_root);
            } 
            
            else {
                printf("\n\n%s[오류] 데이터 파싱 실패 (잘못된 JSON)%s\n", RED, END);
            }

            enter();
            continue;

        } //닉네임 바꾸기 종료 


        //2. 채팅방 설정
        else if(settings_choice == 2) {

            int is_select = 0;
            printf("\n2. 채팅방 설정을 선택했습니다.\n");
            enter();
            
            while(1) {
                
                system("clear");

                printf("\n%s[ 채팅방 최대 출력 갯수 변경 ]%s\n\n", BOLD, END);
                printf("\n0. 나가기          1. 10줄\n");
                printf("\n2. 20줄            3. 30줄\n");
                printf("\n\n선택 >>>   ");

                scanf("%d",&change_chat_max); //바꿀 채팅 길이 입력함
                while (getchar() != '\n') continue;

                switch(change_chat_max){

                    case 0: 
                        printf("\n0. 나가기를 선택했습니다.");
                        enter();
                        break;
                    case 1:
                        chat_display_limit = 10;
                        is_select = 1;
                        break;
                    case 2:
                        chat_display_limit = 20;
                        is_select = 1;
                        break;
                    case 3:
                        chat_display_limit = 30;
                        is_select = 1;
                        break;

                    default: 
                        printf("다시 입력해주세요\n"); 
                        enter();
                        continue;
                }

                //나가기 선택했을 시에는 아예 나가야함(다시 개인 설정 선택 받는 곳으로)
                if(is_select == 0) {
                    break;
                }
                
                // 전송 데이터 구성
                cJSON *root = cJSON_CreateObject();
                cJSON_AddNumberToObject(root, "protocol", PRINT_CHATTING);
                cJSON *data = cJSON_CreateObject();
                cJSON_AddNumberToObject(data, "chat_len", chat_display_limit); // 변경할 값 추가!
                cJSON_AddItemToObject(root, "data", data);

                char *packet = cJSON_PrintUnformatted(root);
                char send_buf[BUFFER_SIZE];
                sprintf(send_buf, "%s\n", packet); 
                send(sock, send_buf, strlen(send_buf), 0); // send_buf 전송!
                
                free(packet);
                cJSON_Delete(root);

                // 서버로부터 메시지 수신
                int str_len = recv(sock, settings_msg, BUFFER_SIZE - 1, 0); 
                if (str_len <= 0) {
                    printf("\n\n수신 오류 .. 종료합니다 .. ");
                    continue; // 수신 오류 시 종료
                }
                settings_msg[str_len] = '\0';  // 문자열 종료 문자 추가

                //서버측에서 데이터 받기
                cJSON *recv_root2 = cJSON_Parse(settings_msg);
                cJSON *recv_data2 = cJSON_GetObjectItem(recv_root2, "data");

                int result_int;
                result_int = cJSON_GetObjectItem(recv_data2, "result")->valueint;

                if(result_int != 1) {
                    printf("\n\n최대 채팅 출력 개수 변경에 실패하셨습니다 ...\n");
                    enter();
                    continue;
                }

                printf("\n\n⭕   최대 채팅 출력 개수는 %d개로 변경되었습니다 !", chat_display_limit); //결과 출력
                enter();
                break;
            }
        }


        //3. 메세지 설정
        else if(settings_choice == 3){

            printf("\n3. 메세지 설정을 선택했습니다.\n");
            enter();

            while(1){

                system("clear"); 
                printf("\n%s[ 메세지 삭제 ]%s", BOLD, END);
                printf("\n\n0. 나가기\n");
                printf("1. 읽은 메세지만 삭제\n");
                printf("2. 전체 메세지 삭제\n");
                

                printf("선택 >>>   ");

                scanf("%d", &change_msg_del);
                while(getchar() != '\n'); //입력버퍼 비워줌

                if(change_msg_del == 0){
                    printf("\n나가기를 선택했습니다.\n");
                    enter();
                    break;
                }

                else if(change_msg_del ==1) {

                    int del_choice;     //2차 확인용 결과

                    printf("\n읽은 메세지만 삭제를 선택했습니다.\n");

                    //읽었던 메세지 출력
                    //한번 더 확인?>
                    //삭제 된 후 확인을 위해 있어야 함
                    printf("\n\n%s정말 삭제하시겠습니까?%s\n", BLUE, END);
                    printf("\n1.예    2.아니요\n");
                    printf("\n선택 >>>  ");
                    scanf("%d",&del_choice);
                    while(getchar() != '\n'); //입력버퍼 비워줌

                    if(del_choice == 1) {

                        //서버에 읽은 메세지 삭제 요청하기
                        cJSON *root = cJSON_CreateObject();
                        cJSON_AddNumberToObject(root, "protocol", DELETE_MSG);

                        //아이디도 같이 보냄
                        cJSON *data = cJSON_CreateObject();
                        cJSON_AddStringToObject(data, "user_id", client_id);  //id 보내기
                        cJSON_AddItemToObject(root, "data", data);// 데이터 객체를 루트에 추가

                        char *packet = cJSON_PrintUnformatted(root);
                        char send_buf[BUFFER_SIZE];
                        sprintf(send_buf, "%s\n", packet); // 끝에 \n을 붙여서 패킷 구분
                        send(sock, send_buf, strlen(send_buf), 0); //헤더 담아서 서버에 전달
                        free(packet);
                        cJSON_Delete(root);    

                        printf("\n\n⭕   읽은 메세지가 삭제되었습니다 !"); //결과 출력
                        enter();
                        continue; //메세지 설정 화면으로 돌아감
                    }

                    else if(del_choice == 2) {
                        printf("\n메세지 설정으로 돌아갑니다.\n");
                        enter();
                        continue; //메세지 설정 화면으로 돌아감
                    }
                }

                else if(change_msg_del == 2) {

                    int del_choice;//2차 확인용 결과

                    printf("\n전체 메세지 삭제를 선택했습니다.\n");
                    //전체 메세지 출력
                    //한번 더 확인?>
                    //삭제 된 후 확인을 위해 있어야 함
                    printf("\n\n%s정말 삭제하시겠습니까?%s\n", BLUE, END);
                    printf("\n1.예    2.아니요\n");
                    printf("선택 >>>  ");
                    scanf("%d",&del_choice);
                    while(getchar() != '\n'); //입력버퍼 비워줌

                    if(del_choice == 1){
                        
                        //서버에 읽은 메세지 삭제 요청하기
                        cJSON *root = cJSON_CreateObject();
                        cJSON_AddNumberToObject(root, "protocol", ALL_MSG_DEL);

                        //아이디도 같이 보냄
                        cJSON *data = cJSON_CreateObject();
                        cJSON_AddStringToObject(data, "user_id", client_id);  //id 보내기
                        cJSON_AddItemToObject(root, "data", data);// 데이터 객체를 루트에 추가

                        char *packet = cJSON_PrintUnformatted(root);
                        char send_buf[BUFFER_SIZE];
                        sprintf(send_buf, "%s\n", packet); // 끝에 \n을 붙여서 패킷 구분
                        send(sock, send_buf, strlen(send_buf), 0); //헤더 담아서 서버에 전달
                        free(packet);
                        cJSON_Delete(root);


                        printf("\n\n⭕   모든 메세지는 전부 삭제되었습니다 !"); //결과 출력
                        continue; //메세지 설정 화면으로 돌아감
                    }

                    else if(del_choice == 2){
                        printf("\n메세지 설정으로 돌아갑니다.\n");
                        continue; //메세지 설정 화면으로 돌아감
                    }

                    else {
                        printf("\n다시 입력해주세요 ..\n");
                        enter();
                        continue; //메세지 설정 화면으로 돌아감
                    }

                }
                else {
                    printf("\n다시 입력해주세요 ..\n");
                    enter();
                    continue; //메세지 설정 화면으로 돌아감
                }
            }
        }
        else {
            printf("\n\n다시 입력해주세요 ! ");
            enter();
            continue;
        }
    }
}

//메세지 파트
void message_menu(int sock) {

    char result[20];    //성공 여부
    char name_msg[BUFFER_SIZE]; // 수신 버퍼

    //메세지 보내기에 필요한 함수
    char send_id[20];  //이 안에서 입력받을 id를 담을 곳
    char send_contents[490]; //메세지 입력받을 변수
    int choice;     //나가기, 보내기, 목록 확인을 입력받기 위한 변수

    // 최대 40개를 담을 배열 선언
    MessageInfo msg_list[40];

    while (1)
    {
        system("clear");
        printf("\n%s📍    [ 메세지 ]   📍%s\n", BOLD, END);
        printf("\n0. 나가기    1. 메세지 보내기    2. 메세지 목록");
        printf("\n\n입력 >>>>   ");
        scanf("%d", &choice);
        while(getchar() != '\n'); // 입력 버퍼 비우기

        // 0. 나가기 선택
        if(choice == 0) {
            printf("\n\n0. 나가기를 선택하셨습니다.");
            enter();
            return;
        }

        // 1. 메세지 보내기 선택(보내는 사람이 맞게 들어가는지 확인 필요)
        else if(choice == 1) {
            printf("\n\n1. 메세지 보내기를 선택하셨습니다. ");
            enter();
            system("clear");

            printf("\n보낼 분의 id를 입력해주세요 >>>  ");          //id만 입력받게 하기
            fgets(send_id, sizeof(send_id), stdin);
            send_id[strcspn(send_id, "\n")] = 0;

            // "/c"입력 시 입력 취소 => 다시 메세지 처음 화면으로 돌아가기
            if(input_c_out(send_id)) continue;

            //id 찾기 신호 전송
            cJSON *root = cJSON_CreateObject();
            cJSON_AddNumberToObject(root, "protocol", FIND_ID); 

            cJSON *data = cJSON_CreateObject(); // 데이터 객체 생성
            cJSON_AddStringToObject(data, "user_id", send_id);

            cJSON_AddItemToObject(root, "data", data);  // 데이터 객체를 루트에 추가

            char *packet = cJSON_PrintUnformatted(root);
            char send_buf[BUFFER_SIZE];
            sprintf(send_buf, "%s\n", packet); // 끝에 \n을 붙여서 패킷 구분
            send(sock, send_buf, strlen(send_buf), 0); //헤더 담아서 서버에 전달
            free(packet);
            cJSON_Delete(root);

            // 2. 서버의 응답을 기다리기
            int total_len = 0;
            memset(name_msg, 0, BUFFER_SIZE);

            while (1) {
                // 일단 크게 한 번 받습니다.
                int str_len = recv(sock, &name_msg[total_len], BUFFER_SIZE - total_len - 1, 0);
                
                if (str_len <= 0) {
                    printf("\n\n수신 오류 또는 연결 끊김 ..");
                    return;
                }
                
                total_len += str_len;
                name_msg[total_len] = '\0';

                // [핵심] 서버가 보낸 마지막 문자가 '\n'인지 확인 (패킷의 끝)
                if (name_msg[total_len - 1] == '\n') {
                    name_msg[total_len - 1] = '\0'; // \n을 널 문자로 바꿔서 JSON 완성
                    break;
                }
                
                // 버퍼가 꽉 찼는데도 \n이 없으면 일단 탈출 (데이터가 너무 큰 경우)
                if (total_len >= BUFFER_SIZE - 1) break;
            }

            // 이제 name_msg에는 20개든 30개든 서버가 보낸 전체 데이터가 다 들어있습니다.
            cJSON *root_msg = cJSON_Parse(name_msg);
            if (root_msg == NULL) {
                // 만약 여기서 실패한다면 서버가 데이터를 다 못 보낸 것입니다.
                printf("\n[디버그] 파싱 실패! 수신된 총 길이: %d", total_len);
                enter();
                continue;
            }

            cJSON *find_respone = cJSON_Parse(name_msg);
            cJSON *protocol_data = cJSON_GetObjectItem(find_respone, "protocol");
            cJSON *id_data = cJSON_GetObjectItem(find_respone, "data");
            
            int result_protocol = protocol_data->valueint;  //성공여부
            
            
            switch (result_protocol)
            {
            case FAIL_FINE_ID:
                printf("\n\n❗   [%s]는 없는 아이디입니다", send_id);
                enter();
                continue;

            case SUCCESS_FINE_ID:
                printf("\n⭕   [%s]님에게 메세지를 보내실 수 있습니다.", send_id);
                strcpy(send_id, cJSON_GetObjectItem(id_data, "user_id")->valuestring);   //찾은 id
                enter();
                break;
            
            default:
                printf("\n\n응답 받기 실패");
                enter();
                continue;
            }

            // --- 성공 시 메시지 입력 ---

            while(1) {

                system("clear");

                char temp_contents[600]; // 입력을 넉넉하게 받을 임시 버퍼
                printf("\n\n받는 사람 : %s", send_id);

                printf("\n\n(입력을 다 하셨으면 enter을 눌러주세요 !)");
                printf("\n메세지 내용(490Bytes 이하) >> ");

                // 1. 입력 받기
                if (fgets(temp_contents, sizeof(temp_contents), stdin) == NULL) break;
                temp_contents[strcspn(temp_contents, "\n")] = 0; // 엔터 제거

                // 2. 글자 수(바이트) 체크
                size_t byte_len = strlen(temp_contents);

                if (byte_len == 0) {
                    printf("%s[오류]: 내용을 입력해야 합니다!%s\n", RED, END);
                    continue;
                }

                if (byte_len >= sizeof(send_contents)) {
                    printf("%s[오류]: 메세지가 너무 깁니다.%s (현재 %zu바이트 / 최대 490바이트)\n", RED, END, byte_len);
                    printf("참고: 한글은 한 글자에 3바이트를 차지합니다.\n");
                    continue;
                }

                //성공 했다면 제대로 변수에 담기
                strcpy(send_contents, temp_contents);
                enter();

                //메세지 보내기 신호 전송
                cJSON *root = cJSON_CreateObject();
                cJSON_AddNumberToObject(root, "protocol", SEND_MSG); 

                // 보내는 사람 id도 같이 주기
                cJSON *data = cJSON_CreateObject(); // 데이터 객체 생성
                cJSON_AddStringToObject(data, "user_id", client_id);
                cJSON_AddStringToObject(data, "to_id", send_id);
                cJSON_AddStringToObject(data, "contents", send_contents); 

                cJSON_AddItemToObject(root, "data", data);  // 데이터 객체를 루트에 추가

                char *packet = cJSON_PrintUnformatted(root);
                char send_buf[BUFFER_SIZE];
                sprintf(send_buf, "%s\n", packet); // 끝에 \n을 붙여서 패킷 구분
                send(sock, send_buf, strlen(send_buf), 0); //헤더 담아서 서버에 전달
                free(packet);
                cJSON_Delete(root);

                // 단순히 recv 한 번이 아니라, 개행까지 완전히 비워줘야 함.
                int ack_total = 0;
                char ack_buf[BUFFER_SIZE];
                while (1) {
                    int str_len = recv(sock, &ack_buf[ack_total], BUFFER_SIZE - ack_total - 1, 0);
                    if (str_len <= 0) break;
                    
                    ack_total += str_len;
                    if (ack_buf[ack_total - 1] == '\n') break; // 개행을 만날 때까지 대기
                }

                printf("\n\n⭕   메세지 전송이 완료되었습니다 !");

                enter();
                break;
            }
        }

        //2. 메세지 목록 선택
        else if(choice == 2) {

            int msg_count = 0; // 실제로 받은 메시지 개수 저장용
            int current_page = 0; // 현재 보고 있는 페이지 (0: 1~20번, 1: 21~40번)
            printf("\n\n📭   메세지 목록을 선택하셨습니다. ");
            enter();

            bool reload_needed = true; // 서버로부터 데이터를 다시 받아야 하는지 여부

            //목록에서 특정 메세지를 선택하여 읽었을 때, 읽음 표시로 바꾸기 위해 서버에서 전송을 해줘야함
            //이때, 서버측의 데이터를 변경했기 때문에 다시 파싱하여 목록 보여주게 하기
            while(1) {

                system("clear");

                if (reload_needed) {

                    cJSON *root = cJSON_CreateObject();
                    cJSON_AddNumberToObject(root, "protocol", MSG_LIST); 

                    cJSON *data = cJSON_CreateObject(); // 데이터 객체 생성
                    cJSON_AddStringToObject(data, "user_id", client_id);
                    cJSON_AddItemToObject(root, "data", data);  // 데이터 객체를 루트에 추가

                    char *packet = cJSON_PrintUnformatted(root);
                    char send_buf[BUFFER_SIZE];
                    sprintf(send_buf, "%s\n", packet); // 끝에 \n을 붙여서 패킷 구분
                    send(sock, send_buf, strlen(send_buf), 0); //헤더 담아서 서버에 전달
                    free(packet);
                    cJSON_Delete(root);

                    // 2. 서버의 응답을 기다리기
                    int total_len = 0;
                    memset(name_msg, 0, BUFFER_SIZE);

                    while (1) {
                        // 일단 크게 한 번 받습니다.
                        int str_len = recv(sock, &name_msg[total_len], BUFFER_SIZE - total_len - 1, 0);
                        
                        if (str_len <= 0) {
                            printf("\n\n수신 오류 또는 연결 끊김 ..");
                            return;
                        }
                        
                        total_len += str_len;
                        name_msg[total_len] = '\0';

                        // [핵심] 서버가 보낸 마지막 문자가 '\n'인지 확인 (패킷의 끝)
                        if (name_msg[total_len - 1] == '\n') {
                            name_msg[total_len - 1] = '\0'; // \n을 널 문자로 바꿔서 JSON 완성
                            break;
                        }
                        
                        // 버퍼가 꽉 찼는데도 \n이 없으면 일단 탈출 (데이터가 너무 큰 경우)
                        if (total_len >= BUFFER_SIZE - 1) break;
                    }


                    // 파싱 시작
                    cJSON *root_msg = cJSON_Parse(name_msg);
                    if (root_msg != NULL) {
                        cJSON *msg_list_data = cJSON_GetObjectItem(root_msg, "data");
                        cJSON *json_list = cJSON_GetObjectItem(msg_list_data, "messages");
                        
                        if (json_list != NULL) {
                            msg_count = cJSON_GetArraySize(json_list);
                            if (msg_count > 40) msg_count = 40;

                            for (int i = 0; i < msg_count; i++) {
                                cJSON *item = cJSON_GetArrayItem(json_list, i);
                                if (!item) continue;

                                cJSON *msg_id_obj = cJSON_GetObjectItem(item, "msg_id");
                                cJSON *from_id_obj = cJSON_GetObjectItem(item, "from_id");
                                cJSON *contents_obj = cJSON_GetObjectItem(item, "recv_contents");
                                cJSON *time_obj = cJSON_GetObjectItem(item, "recv_day_time");
                                cJSON *read_obj = cJSON_GetObjectItem(item, "read_or_not");

                                // [안전 장치] strncpy로 교체하여 메모리 오염 방지 (sizeof 중요!)
                                if (msg_id_obj) sprintf(msg_list[i].msg_id, "%d", msg_id_obj->valueint);
                                if (from_id_obj) strncpy(msg_list[i].from_id, from_id_obj->valuestring, sizeof(msg_list[i].from_id)-1);
                                
                                if (contents_obj) {
                                    // 내용 복사 안전하게
                                    strncpy(msg_list[i].recv_contents, contents_obj->valuestring, sizeof(msg_list[i].recv_contents)-1);
                                    msg_list[i].recv_contents[sizeof(msg_list[i].recv_contents)-1] = '\0'; // 널 문자 보장

                                    // 미리보기 생성
                                    strncpy(msg_list[i].pre_contents, msg_list[i].recv_contents, 46);
                                    msg_list[i].pre_contents[46] = '\0';
                                    if (strlen(msg_list[i].recv_contents) > 46) strcat(msg_list[i].pre_contents, "...");
                                }
                                
                                if (time_obj) strncpy(msg_list[i].recv_day_time, time_obj->valuestring, sizeof(msg_list[i].recv_day_time)-1);
                                if (read_obj) strncpy(msg_list[i].read_or_not, read_obj->valuestring, sizeof(msg_list[i].read_or_not)-1);
                            }
                        }
                        cJSON_Delete(root_msg); // 여기서 삭제
                        reload_needed = false; // 성공적으로 로드됨

                    } else {
                        // 파싱 실패 시 (찌꺼기 데이터 때문일 확률 높음)
                        // 한 번 더 시도하거나 오류 메시지 출력
                        printf("\n데이터 해석 오류! (수신된 길이: %d)", total_len);
                        // enter();
                        reload_needed = true; 
                        continue; 
                    }
                }

                //한 화면에 최대 20개만 나오게 하는 부분 추가해야함
                int start_idx = current_page * 20;
                int end_idx = start_idx + 20;
                if (end_idx > msg_count) end_idx = msg_count;
            
                //수정 
                //출력 예시) 1. 안녕하신가?  보낸 사람 : 이지나  보낸 날짜 : 2026-01-28 00:33  (안읽음)
                printf("\n📭%s   [ 메시지 목록 ]  %s📭   (총 %d개 / %d 페이지)\n", BOLD, END, msg_count, current_page + 1);
                printf("\n\n============================================================================================================================\n\n");
                for (int i = start_idx; i < end_idx; i++) {
                    char* status_color = (strcmp(msg_list[i].read_or_not, "안읽음") == 0) ? BLUE : END;

                    printf("\n%-2d. %-60s  보낸 사람 : %-30s보낸 날짜 : %s  (%s%s%s)\n", 
                            i + 1,
                            msg_list[i].pre_contents, 
                            msg_list[i].from_id, 
                            msg_list[i].recv_day_time,
                            status_color,              // 색상 시작
                            msg_list[i].read_or_not,   // 텍스트 출력
                            END);                      // 색상 초기화
                }
                printf("\n\n============================================================================================================================\n");
                printf("\n(0 : 나가기   /  99 : 목록 최신화   /   77 : 이전 목록   /   88 : 다음 목록)");
                printf("\n\n선택할 메세지 번호 >>>   ");

                int select;
                scanf("%d", &select);
                while(getchar() != '\n');

                if(select == 0) {
                    printf("\n\n0. 뒤로 가기를 선택하셨습니다.");
                    enter();
                    break;
                }

                //99 : 목록 최신화 
                else if(select == 99) {
                    printf("\n\n99. 목록 최신화를 선택하셨습니다.");
                    enter();
                    reload_needed = true; // 이건 서버 갔다 와야 함!
                    continue;
                }

                //77 : 이전 목록
                else if(select == 77) { 
                    if (current_page > 0) current_page--;
                    else { 
                        printf("\n❗   첫 페이지입니다."); 
                        enter(); 
                    }
                    reload_needed = false; //서버 가지 말고 출력만 다시 해!
                    continue;
                    
                }

                //88 : 다음 목록
                else if(select == 88) {
                    if (end_idx < msg_count) current_page++;
                    else { 
                        printf("\n❗   마지막 페이지입니다."); 
                        enter(); 
                    }
                    reload_needed = false; //서버 가지 말고 출력만 다시 해!
                    continue;
                    
                }

                //메세지 id(msg_id) + 클라이언트 id(user_id)=> 읽음 표시로 변경할 수 있게

                else if(select > 0 && select <= msg_count) {
                    int idx = select - 1; // 배열 인덱스 맞추기
                    printf("\n\n%d번 메세지를 선택하셨습니다.", select);
                    enter();
                    system("clear");
                    printf("\n\n[보낸 사람 : %s             보낸 날짜 : %s]", msg_list[idx].from_id, msg_list[idx].recv_day_time);
                    printf("\n\n\n%s", msg_list[idx].recv_contents);

                    //메세지 보내기 신호 전송
                    cJSON *root = cJSON_CreateObject();
                    cJSON_AddNumberToObject(root, "protocol", READ_MSG); 

                    // 보내는 사람 id도 같이 주기
                    cJSON *data = cJSON_CreateObject(); // 데이터 객체 생성
                    cJSON_AddStringToObject(data, "user_id", client_id);
                    cJSON_AddNumberToObject(data, "msg_id", atoi(msg_list[idx].msg_id));
                    cJSON_AddItemToObject(root, "data", data);  // 데이터 객체를 루트에 추가

                    char *packet = cJSON_PrintUnformatted(root);
                    char send_buf[BUFFER_SIZE];
                    sprintf(send_buf, "%s\n", packet); // 끝에 \n을 붙여서 패킷 구분
                    send(sock, send_buf, strlen(send_buf), 0); //헤더 담아서 서버에 전달
                    free(packet);
                    cJSON_Delete(root);

                    printf("\n\n\n(메세지 확인 완료)");
                    enter();
                    reload_needed = true; // 읽음 표시 갱신을 위해 서버 데이터 다시 요청!
                    continue;
                }

                else {
                    printf("\n\n다시 입력해주세요 .. ");
                    enter();
                    continue;
                }
            }
        }

        //외의 것 입력
        else {
            printf("\n\n다시 입력해주세요 !");
            enter();
        }
    }
}


//채팅메뉴
void chat_menu(int sock) {

    ChatRoom room_list[100]; //최대 100개의 방 정보를 담을 배열
    char msg[BUFFER_SIZE];  //메세지 버퍼
    char name_msg[BUFFER_SIZE]; // 수신 버퍼
    int user_input;        //유저 입력 값
    int room_count = 0;    //방 개수 값
    char room_name[50];   //방 이름 저장
    char room_password[20];  //방 비밀번호 저장
    char make_room[20];      // 방 생성 성공 여부를 받을 변수
    int room_idx;                    //들어갈 방 번호

    while (1)
    {
        system("clear");
        printf("\n%s📍   채팅  📍%s\n", BOLD, END);
        printf("\n0.나가기     1.채팅방 찾기     2. 채팅방 생성");
        printf("\n\n입력 >>>   ");
        scanf("%d", &user_input);
        while(getchar() != '\n'); // 입력 버퍼 비우기

        if(user_input == 0) 
        {
            printf("\n\n0. 나가기를 선택하셨습니다.\n");
            enter();
            break;
        }
        
        //1. 채팅방 찾기 선택 (목록 불러오기 -> 선택해서 들어가기)
        else if(user_input == 1)
        {
            printf("\n\n1. 채팅방 찾기를 선택하셨습니다.\n");

            while(1) {

                //서버에 채팅방 찾기 전송
                cJSON *call = cJSON_CreateObject();
                cJSON_AddNumberToObject(call, "protocol", CHATTING_LIST); 
                char *packet = cJSON_PrintUnformatted(call);
                sprintf(msg, "%s\n", packet);
                send(sock, msg, strlen(msg), 0);
                free(packet);
                cJSON_Delete(call);

                int total_len = 0;
                memset(name_msg, 0, BUFFER_SIZE);
                while (1) {
                    int str_len = recv(sock, &name_msg[total_len], BUFFER_SIZE - total_len - 1, 0);
                    if (str_len <= 0) return;
                    total_len += str_len;
                    name_msg[total_len] = '\0';
                    if (name_msg[total_len - 1] == '\n') {
                        name_msg[total_len - 1] = '\0';
                        break;
                    }
                }
                
                cJSON *find_chatroom = cJSON_Parse(name_msg);
                if (find_chatroom == NULL) continue;

                cJSON *json_list = cJSON_GetObjectItem(find_chatroom, "rooms");
                room_count = (json_list != NULL) ? cJSON_GetArraySize(json_list) : 0;
                if (room_count > 100) room_count = 100;

                for (int i = 0; i < room_count; i++) {
                    cJSON *item = cJSON_GetArrayItem(json_list, i);
                    room_list[i].room_id = cJSON_GetObjectItem(item, "room_id")->valueint;
                    strncpy(room_list[i].room_title, cJSON_GetObjectItem(item, "room_title")->valuestring, sizeof(room_list[i].room_title)-1);
                    strncpy(room_list[i].password, cJSON_GetObjectItem(item, "password")->valuestring, sizeof(room_list[i].password)-1);                    
                    room_list[i].user_count = cJSON_GetObjectItem(item, "user_count")->valueint;
                }
                cJSON_Delete(find_chatroom);

                enter();
                system("clear");
                
                // 깔끔한 헤더 구성
                printf("\n    📋   %s[ 채팅방 선택 ]%s  📋\n", BOLD, END);
                printf("  ------------------------------------------------------------------------\n");
                printf("  %-6s  %-50s  %-10s  %-8s\n", "ID", "방 제목", "상태", "인원");
                printf("  ------------------------------------------------------------------------\n");

                if (room_count == 0) {
                    printf("\n\n🗨   현재 개설된 방이 없습니다.\n");
                    enter();
                    break;
                } 

                for (int i = 0; i < room_count; i++) {
                    // 비밀번호 유무 (잠금/공개) 색상 적용
                    char lock_status[30];
                    if (strcmp(room_list[i].password, "") != 0) 
                        sprintf(lock_status, "%s[잠금]%s", RED, END);
                    else sprintf(lock_status, "%s[공개]%s", GREEN, END);
                    
                    // 출력 (id, 제목, 잠금여부, 인원)
                    printf("  %-6d  %-50s  %-10s  (%d/10)\n",
                        i + 1,                     // 방 순서
                        room_list[i].room_title,   // 방 제목
                        lock_status,               // 공개/잠금 상태
                        room_list[i].user_count);  // 현재 인원
                }

                printf("  ------------------------------------------------------------------------\n");
                printf("  (0: 나가기  /  99: 새로고침)\n");

                int input_no;
                int input_pw;

                printf("\n  입장할 방 번호 입력 >>> ");
                scanf("%d", &input_no);
                while(getchar() != '\n');

                //0. 나가기
                if(input_no == 0) {
                    printf("\n0번 나가기를 선택하셨습니다.");
                    enter();
                    break;
                }

                //새로고침
                else if(input_no == 99) continue;

                else if(input_no > 0 && input_no <= room_count) {
                    int idx = input_no - 1;
                    
                    // [비밀번호 체크 로직]
                    if (strcmp(room_list[idx].password, "") != 0) {
                        char input_pw[20];
                        printf("\n\n\n🔒  이 방은 비밀번호가 있습니다. \n입력 >>>   ");
                        fgets(input_pw, sizeof(input_pw), stdin);
                        input_pw[strcspn(input_pw, "\n")] = 0;

                        if (strcmp(room_list[idx].password, input_pw) != 0) {
                            printf("\n  ❌ 비밀번호가 틀렸습니다!\n");
                            continue;
                        }
                    }

                    // [서버에 입장 요청 전송]
                    cJSON *join_req = cJSON_CreateObject();
                    cJSON_AddNumberToObject(join_req, "protocol", ROOM_JOIN_REQ);
                    cJSON *j_data = cJSON_CreateObject();
                    cJSON_AddNumberToObject(j_data, "room_id", room_list[idx].room_id);
                    cJSON_AddStringToObject(j_data, "user_id", client_id);
                    cJSON_AddItemToObject(join_req, "data", j_data);

                    char *j_packet = cJSON_PrintUnformatted(join_req);
                    sprintf(msg, "%s\n", j_packet);
                    send(sock, msg, strlen(msg), 0);
                    free(j_packet);
                    cJSON_Delete(join_req);

                    // [입장 결과 수신]
                    memset(name_msg, 0, BUFFER_SIZE);
                    int r_len = recv(sock, name_msg, BUFFER_SIZE - 1, 0);
                    if (r_len > 0) {
                        name_msg[r_len] = '\0';
                        cJSON *go_room = cJSON_Parse(name_msg);
                        cJSON *gogo_room = cJSON_GetObjectItem(go_room, "data");
                        if (gogo_room && strcmp(cJSON_GetObjectItem(gogo_room, "result")->valuestring, "성공") == 0) {
                            printf("\n  ✅ 입장 성공! 채팅방으로 이동합니다.\n");
                            enter();
                            cJSON_Delete(go_room);
                            in_room_chat(sock); // 채팅창 루프 진입!
                            break; 
                        } 
                        else {
                            printf("\n  ❌ 입장에 실패했습니다. (방이 꽉 찼거나 사라짐)\n");
                            cJSON_Delete(go_room);
                        }
                    }
                } 
                else {
                    printf("\n  ❌ 잘못된 번호입니다.\n");
                    enter();
                }
            }  
        } 

        // 2. 채팅방 생성 선택
        else if(user_input == 2)
        {
            char room_temp[150]; // 입력을 넉넉하게 받을 임시 버퍼
            printf("\n\n2. 채팅방 생성을 선택하셨습니다.\n");

            while(1) {
                
                enter();
                system("clear");

                printf("\n\n생성할 방 이름 입력(50Bytes 이내) >>>   ");

                fgets(room_temp, sizeof(room_temp), stdin);
                room_temp[strcspn(room_temp, "\n")] = 0;

                // 1. "/c" 입력 시 처리
                // 입력값 중에 /c가 "어디라도" 들어있으면 취소로 인식
                if(input_c_out(room_temp)) {
                    printf("\n생성을 취소합니다.\n");
                    enter();
                    continue;
                }  

                size_t room_name_len = strlen(room_temp);

                // 2. 글자 수(바이트) 체크
                if (room_name_len == 0) {
                    printf("%s[오류] 최소 한 글자 이상 입력해주세요!%s\n", RED, END);
                    enter();
                    continue;
                }

                // 3. 실제 저장 공간(room_name은 50바이트이므로)보다 크면 잘라내거나 에러 처리
                if (room_name_len >= 50) {
                    printf("%s[오류] 너무 길어서 저장할 수 없습니다! (최대 50바이트)%s\n", RED, END);
                    enter();
                    continue;
                }

                strcpy(room_name, room_temp); // 검증된 room_temp를 실제 목적지인 room_name에 복사

                //비밀번호 설정 안하고 싶을 수도 있으니깐 문자열로 받기
                char pw_temp[20];

                printf("\n\n비밀번호 설정 (숫자 4자리, 안 하려면 그냥 엔터) >>> ");
                fgets(pw_temp, sizeof(pw_temp), stdin);
                pw_temp[strcspn(pw_temp, "\n")] = 0;

                if (strlen(pw_temp) == 0) {
                    strcpy(room_password, ""); // 비밀번호 없음
                    break;
                } 
                else {
                    int is_all_digit = 1; // 숫자인지 판별하는 플래그

                    // 1. 길이 체크 (4자리인지)
                    if (strlen(pw_temp) != 4) {
                        is_all_digit = 0;
                    } 
                    else {
                        // 2. 각 문자가 숫자인지 하나씩 확인
                        for (int i = 0; i < 4; i++) {
                            if (!isdigit(pw_temp[i])) {
                                is_all_digit = 0;
                                break;
                            }
                        }
                    }

                    if (!is_all_digit) {
                        printf("\n\n비밀번호는 숫자 4자리 입니다 !");
                        // enter(); // 사용자 정의 함수로 추정
                        continue;
                    }
                     
                    else {
                        strcpy(room_password, pw_temp);
                        break;
                    }
                }
            }

            //방 만들었다는 신호 보내기
            cJSON *box = cJSON_CreateObject();
            cJSON_AddNumberToObject(box, "protocol", CATTING_ROOM_OPEN); 

            cJSON *data = cJSON_CreateObject(); // 데이터 객체 생성

            cJSON_AddStringToObject(data, "user_id", client_id);  //방 만든사람 아이디도 보내기
            cJSON_AddStringToObject(data, "room_title", room_name);  //여기에 방이름 입력받기
            cJSON_AddStringToObject(data, "password", room_password);    // 비번 입력 받아서 추가

            cJSON_AddItemToObject(box, "data", data);  // 데이터 객체를 루트에 추가

            char *packet = cJSON_PrintUnformatted(box);
            char send_buf[BUFFER_SIZE];
            sprintf(send_buf, "%s\n", packet); // ✅ \n 추가
            send(sock, send_buf, strlen(send_buf), 0);
            
            free(packet);
            cJSON_Delete(box);

            // 2. 서버의 응답을 기다리기
            int total_len = 0;
            memset(name_msg, 0, BUFFER_SIZE);

            while (1) {
                // 일단 크게 한 번 받습니다.
                int str_len = recv(sock, &name_msg[total_len], BUFFER_SIZE - total_len - 1, 0);
                
                if (str_len <= 0) {
                    printf("\n\n수신 오류 또는 연결 끊김 ..");
                    return;
                }
                
                total_len += str_len;
                name_msg[total_len] = '\0';

                // [핵심] 서버가 보낸 마지막 문자가 '\n'인지 확인 (패킷의 끝)
                if (name_msg[total_len - 1] == '\n') {
                    name_msg[total_len - 1] = '\0'; // \n을 널 문자로 바꿔서 JSON 완성
                    break;
                }
                
                // 버퍼가 꽉 찼는데도 \n이 없으면 일단 탈출 (데이터가 너무 큰 경우)
                if (total_len >= BUFFER_SIZE - 1) break;
            }

            // 이제 name_msg에는 20개든 30개든 서버가 보낸 전체 데이터가 다 들어있습니다.
            cJSON *root_msg = cJSON_Parse(name_msg);
            if (root_msg == NULL) {
                // 만약 여기서 실패한다면 서버가 데이터를 다 못 보낸 것입니다.
                printf("\n[디버그] 파싱 실패! 수신된 총 길이: %d", total_len);
                enter();
                continue;
            }

            cJSON *open_room_data = cJSON_GetObjectItem(root_msg, "protocol");
            
            int result_protocol = open_room_data->valueint;  //성공여부
            
            switch (result_protocol)
            {
            case ACK_FAIL_ROOM_OPEN:
                printf("\n\n방 생성에 실패하셨습니다 .. ");
                enter();
                continue;

            case ACK_ROOM_OPEN:
                printf("\n\n방 생성에 성공하셨습니다 ! ");
                enter();
                in_room_chat(sock);
                break;
            
            default:
                printf("\n\n응답 받기 실패");
                enter();
                continue;
            }
            cJSON_Delete(root_msg);
        }
    }
    return;
}

//[닉네임] : 대화내용 형태 , 본인 채팅은 초록색으로 표시
// 남이 보낸 메시지를 실시간으로 출력하는 스레드 함수
void *recv_room_msg(void *arg) {
    int sock = *((int *)arg);
    char buf[BUFFER_SIZE];
    while (1) {

        //서버로부터 받기
        int str_len = recv(sock, buf, BUFFER_SIZE - 1, 0);
        if (str_len <= 0) return NULL;
        buf[str_len] = 0;

        cJSON *root = cJSON_Parse(buf);
        if (!root) continue;
        
        cJSON *protocol = cJSON_GetObjectItem(root, "protocol");
        if (protocol->valueint == CHATTING_MSG) {

            cJSON *data = cJSON_GetObjectItem(root, "data");

            if (data) {
                //상대방의 정보를 가져온 것
                char *sender_id = cJSON_GetObjectItem(data, "sender_id")->valuestring;
                char *sender_nick = cJSON_GetObjectItem(data, "sender_nick")->valuestring;
                char *message = cJSON_GetObjectItem(data, "message")->valuestring;
                char sender[40] = {0};      // 상대 표시 이름
                char my_name[40] = {0};       // 내 표시 이름

                // 상대방 닉네임 판별(닉네임 없으면 아이디 출력)
                if(strcmp(sender_nick, "") == 0) 
                    strncpy(sender, sender_id, sizeof(sender)-1);
                else strncpy(sender, sender_nick, sizeof(sender)-1);
                

                //  내 이름 판별 (닉네임 없으면 아이디 출력)
                if(strcmp(client_nickname, "") == 0)
                    strncpy(my_name, client_id, sizeof(my_name) - 1);
                else strncpy(my_name, client_nickname, sizeof(my_name) - 1);
                
                char temp_msg[512];

                // 내 전역변수 client_id와 비교해서 본인이면 초록색 출력, 아니면 그대로 출력
                // [닉네임] : 메시지
                // 1. 먼저 출력될 문자열을 temp_msg에 예쁘게 담습니다.
                if (strcmp(sender_id, client_id) == 0)
                    sprintf(temp_msg, "%s[%s] : %s%s", GREEN, my_name, message, END);
                else 
                    sprintf(temp_msg, "[%s] : %s", sender, message);

                // 2. 히스토리에 추가하고 화면을 다시 렌더링합니다.
                push_chat_history(temp_msg); 
                render_chat_view(); // 여기서 system("clear") 후 히스토리 전체를 다시 출력함
                
                fflush(stdout);
            }
        }
        else if (protocol->valueint == ACK_ROOM_CLOSE) {
            printf("\n%s[알림] 방장이 나갔습니다. 대기실로 이동합니다.%s\n", RED, END);
            stop_chat_flag = 1;
            cJSON_Delete(root);
            break; // 루프 탈출
        }
        else if (protocol->valueint == ACK_EXIT_ROOM) { //그냥 클라이언트가 나갔을 때(방장 X)
            stop_chat_flag = 1;
            cJSON_Delete(root);
            break;
        }
        cJSON_Delete(root);
    }
    return NULL;
}

// 실제 채팅창 화면 로직
void in_room_chat(int sock) {

    pthread_t thread;
    stop_chat_flag = 0; // 진입 시 플래그 초기화

    history_cnt = 0; // 채팅방 입장 시 히스토리 초기화
    render_chat_view(); // 초기 화면 그리기
    
    // 메시지 수신 전용 스레드 시작 (서버가 보내는 채팅 및 '방 종료' 알림 감시)
    if (pthread_create(&thread, NULL, recv_room_msg, (void *)&sock) != 0) {
        printf("[오류] 수신 스레드 생성 실패\n");
        return;
    }
    pthread_detach(thread); 

    char chat_buf[512];
    
    while (1) {
        // 루프 시작 시 항상 플래그 체크
        // 방장이 나갔을 때 recv_room_msg 스레드에서 이 플래그를 1로 만듦
        if (stop_chat_flag) break; 

        // 입력을 받기 전 커서를 항상 입력창 위치로 이동
        gotoxy(28, chat_display_limit + 3);

        if (fgets(chat_buf, sizeof(chat_buf), stdin) == NULL) break;  // [참고] fgets는 사용자가 엔터를 칠 때까지 여기서 멈춤(Blocking)
        fflush(stdout); // 프롬프트 즉시 출력 보장

        chat_buf[strcspn(chat_buf, "\n")] = 0;

        // 입력 후에도 플래그 체크 (입력 대기 중에 방이 터졌을 수 있음)
        if (stop_chat_flag) break;

        // 1. 퇴장 처리
        if (strcmp(chat_buf, "/q") == 0) {
            cJSON *root = cJSON_CreateObject();
            cJSON_AddNumberToObject(root, "protocol", EXIT_ROOM);
            char *p = cJSON_PrintUnformatted(root);
            send(sock, p, strlen(p), 0);
            free(p);
            cJSON_Delete(root);

            break;
        }

        // 2. 빈 메시지 전송 방지
        if (strlen(chat_buf) == 0) continue;

        // 3. 채팅 메시지 전송
        cJSON *root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "protocol", CHATTING_MSG);
        cJSON *data = cJSON_CreateObject();
        cJSON_AddStringToObject(data, "message", chat_buf);
        cJSON_AddItemToObject(root, "data", data);
        
        char *p = cJSON_PrintUnformatted(root);
        char packet[BUFFER_SIZE];
        sprintf(packet, "%s\n", p); // 서버가 파싱하기 쉽게 개행 포함
        send(sock, packet, strlen(packet), 0);
        
        free(p);
        cJSON_Delete(root);
    }
    printf("\n[시스템] 대기실로 이동합니다.\n");
}


//메인 시작
int main(int argc, char* argv[]) 
{
    //============================서버와 연결===================
    int sock;
    struct sockaddr_in addr;
    int port = 5003;

    sock = socket(PF_INET, SOCK_STREAM, 0);


    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(port);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) 
    {   
        error_handling("connect() error");
    }

    //=============================회원가입 / 로그인 화면======================
    while(1) {
        system("clear");

        printf("%s====================== [ GOOOOOOD TALK ] ======================%s\n", BOLD, END);
        printf("\n\n 0. 종료     1. 회원가입      2. 로그인");
        printf("\n\n[입력] >>   ");

        int choice;
        scanf("%d", &choice);
        while(getchar() != '\n'); // 입력 버퍼 비우기

        if(choice == 0) {
            printf("\n\n0. 종료를 선택하셨습니다 .. ");
            printf("\nBye Bye .... ");
            enter();
            exit(0);
        }
        
        //1. 회원가입 선택
        else if(choice == 1) {
            printf("\n\n1. 회원가입을 선택하셨습니다. ");
            enter();
            member_ship(sock);
        }
        
        //2. 로그인 선택
        else if(choice == 2) {
            printf("\n\n2. 로그인을 선택하셨습니다. ");
            enter();
            if(login_user(sock)) continue;
            else break;
        }

        //그 외의 선택은 다시 입력받게하기
        else {
            printf("\n\n❌   다시 입력해주세요 ..");
            enter();
            continue;
        }
    }
    //==========================================================================


    //=============================초기화면(채팅/메세지/개인설정/나가기)=======================
    while(1) {
        system("clear");

        printf("%s====================== [ GOOOOOOD TALK ] ======================%s\n", BOLD, END);
        printf("\n\n         아이디 : %s        닉네임 : %s", client_id, client_nickname);
        printf("\n\n 0. 로그아웃     1. 채팅      2. 메세지      3. 개인설정");
        printf("\n\n[입력] >>   ");

        int choice;
        scanf("%d", &choice);
        while(getchar() != '\n'); // 입력 버퍼 비우기

        if(choice == 0) {
            printf("\n\n0. 로그아웃을 선택하셨습니다 .. ");
            printf("\nBye Bye .... ");
            enter();
            exit(0);
        }

        //1. 채팅
        else if(choice == 1) {
            printf("\n\n1. 채팅을 선택하셨습니다.");
            enter();
            chat_menu(sock);
            continue;
        }

        //2. 메세지
        else if(choice == 2) {
            printf("\n\n2. 메세지를 선택하셨습니다.");
            enter();
            message_menu(sock);
            continue;
        }

        //3. 개인설정
        else if(choice == 3) {
            printf("\n\n3. 개인 설정을 선택하셨습니다.");
            enter();
            user_setting(sock);
            continue;
        }

        //그 외의 선택은 다시 입력받게하기
        else {
            printf("\n\n❌   다시 입력해주세요 ..");
            enter();
            continue;
        }
    }
    
    return 0;
}



void error_handling(char *message)// 에러 처리 함수
{
	fputs(message, stderr);  // 표준 에러 출력 스트림에 메시지 출력
	fputc('\n', stderr);   // 표준 에러 출력 스트림에 개행 문자 출력
	exit(1);   // 비정상 프로그램 종료
}


