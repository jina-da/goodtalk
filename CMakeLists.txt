# 1. CMake 버전 설정
cmake_minimum_required(VERSION 3.10)

# 2. 프로젝트 정보
project(HiRunningDay VERSION 1.0)

# 3. C++ 표준 설정 (최신 문법을 쓰기 위해 17버전 추천)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- 1. 서버 프로그램 만들기 ---
# server_main.cpp와 필요한 부품들을 합쳐서 'talk_server' 생성
add_executable(goodtalk_server 
    server_project.c 
    cJSON.h  # 보상 로직이 서버에 필요하다면 추가
    cJSON.c
    protocol_header.h
)

# --- 2. 클라이언트 프로그램 만들기 ---
# client_main.cpp를 가지고 'talk_client' 생성
add_executable(goodtalk_client 
    client_project.c
    protocol_header.h
    cJSON.h
    cJSON.c
)

# 터미널에서 cd GOOOD_TALK 폴더에 들어가서 
# mkdir build (build 폴더 만들기)
# cd build 들어가기
# cmake ..
# make