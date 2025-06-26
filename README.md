# rsp_ppippi

사용법

- LCD 드라이버 설치 명령

    ```bash
        
    $ sudo insmod mylcd.ko
    $ sudo mknod /dev/mylcd c $(grep mylcd /proc/devices | awk '{print $1}') 0

    ```

- 컴파일 명령

    ```bash

    $ gcc client.c -o client keypad.c -Wall -pthread && 
        gcc -Wall -pthread -o server server.c

    ```

- 서버 실행

    ```bash

    $ ./server

    ```


- 클라이언트 실행

    ```bash

    $ sudo ./client <server ip 주소>

    ```