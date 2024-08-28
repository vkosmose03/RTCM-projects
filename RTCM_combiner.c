#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <pthread.h>

pthreads_mutex_t lock;
uint8_t *bit29, *bit30;
uint16_t *Z1, *Z2;
uint32_t *seq;
*seq = 0;
*bit29 = 1;
*bit30 = 1;


uint8_t wordXor(uint32_t word){
    char bit = 0;
    uint32_t buf = word;
    for(int i = 0; i < 30; i++){
        bit = bit ^ (buf & 0x01);
        buf >>= 1;
    }
    return bit;
}

uint8_t ParityCreate(uint32_t word, uint8_t Bit30, uint8_t Bit29){
    uint8_t cod = 0;
    uint32_t bufWord = word;
    cod += wordXor(bufWord & 0x3b1f3480) ^ Bit29;
    cod <<= 1;
    cod += wordXor(bufWord & 0x1d8f9a40) ^ Bit30;
    cod <<= 1;
    cod += wordXor(bufWord & 0x2ec7cd00) ^ Bit29;
    cod <<= 1;
    cod += wordXor(bufWord & 0x1763e680) ^ Bit30;
    cod <<= 1;
    cod += wordXor(bufWord & 0x2bb1f340) ^ Bit30;
    cod <<= 1;
    cod += wordXor(bufWord & 0x0b7a89c0) ^ Bit29;
    return cod;
}

uint32_t SequencesChanger(uint32_t word) {

    pthreads_mutex_lock(&lock);

    if(*seq == 7) {
        *seq = 0;
    } else {
        seq = seq + 1;
    }

    word = (word & 0xfff8fffc) | ((seq << 14) & 0x0001C000);

    pthreads_mutex_unlock(&lock);
    
    return word;

}

typedef struct {
    bool preamble_result;
    uint8_t preamble_place;
    uint8_t Preamble_priznak;
} Preamble;

Preamble PreambleSearch(uint64_t potok, uint8_t bit30, uint8_t bit29) {
    Preamble pre;
    uint32_t word;
    uint8_t parity;
    bool parity_result = false;
    pre.preamble_result = false;
    for(int f = 0; f < 32; f++) {
        if(bit30 == 0) {
            if(((potok << f) & 0xff00000000000000) == 0x6600000000000000) {
                pre.preamble_place = f;
                pre.Preamble_priznak = 0;
                word = (potok >> (32 - pre.preamble_place)) & 0xffffffff;

                if(bit30 == 1) {
                    word = (~word & 0xffffff00) | word & 0x000000ff;
                }
                parity = ParityCreate(((word >> 2) & 0x3fffffff), bit30, bit29);
                if(((word >> 2) & 0x0000003f) == parity) {
                    parity_result = true;
                }
                if(!parity_result) {
                    continue;
                }

                pre.preamble_result = true;
                break;
            }
        } else if(bit30 == 1) {
            if(((potok << f) & 0xff00000000000000) == 0x9900000000000000) {
                pre.preamble_place = f;
                pre.Preamble_priznak = 1;
                word = (potok >> (32 - pre.preamble_place)) & 0xffffffff;

                if(bit30 == 1) {
                    word = (~word & 0xffffff00) | word & 0x000000ff;
                }
                parity = ParityCreate(((word >> 2) & 0x3fffffff), bit30, bit29);
                if(((word >> 2) & 0x0000003f) == parity) {
                    parity_result = true;
                }
                if(!parity_result) {
                    continue;
                }

                pre.preamble_result = true;
                break;
            }
        }
    }
    return pre;
}

void* firstCOM() {

    HANDLE PORT4 = CreateFile("COM4", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (PORT4 == INVALID_HANDLE_VALUE) {
        printf("Error opening COM4\n");
        Sleep(10000);
        return;
    }

    if (!GetCommState(PORT4, &dcb)) {
        printf("Error getting COM4 port state\n");
        CloseHandle(PORT4);
        Sleep(10000);
        return;
    }

    HANDLE PORT1 = CreateFile("COM1", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (PORT1 == INVALID_HANDLE_VALUE) {
        printf("Error opening COM1\n");
        return 1;
    }

    if (!GetCommState(PORT1, &dcb)) {
        printf("Error getting COM1 port state\n");
        CloseHandle(PORT1);
        return 1;
    }

    uint64_t potok;
    uint32_t word;
    uint32_t message[31];
    uint16_t Z1count;
    uint8_t buffer, buffer_roll;
    uint8_t bit29, bit30, parity;
    uint8_t cut1, cut2, chislo_kadrov;
    bool parity_result;
    DWORD Readed;
    bit29 = 1;
    bit30 = 1;

    while(1) {

        parity_result = false;
        if (!ReadFile(PORT4, buffer, sizeof(buffer), &Readed, NULL)) { // Чтение начало
            printf("Ошибка чтения данных из файла!\n");
            Sleep(10000);
            return 1;
        }
        for(int i = 0; i < 8; i++) {
            uint8_t bit = (buffer >> i) & 0x01;
            if(bit == 1) {
                buffer_roll |= 0x01 << (7 - i);
            }
            bit = 0;
        }
        potok <<= 6;
        potok += (buffer_roll >> 2) & 0x3f;  
        buffer_roll = 0;
        buffer = 0; 
        CloseHandle(PORT4); // Чтение конец

        Preamble preamble = PreambleSearch(potok, bit30, bit29);

        if(!preamble.preamble_result) {
            continue;
        }

        word = (potok >> (32 - preamble.preamble_place)) & 0xffffffff;

        if(bit30 == 1) {
            word = (~word & 0xffffff00) | word & 0x000000ff;
        }

        parity = ParityCreate(((word >> 2) & 0x3fffffff), bit30, bit29);

        if(((word >> 2) & 0x0000003f) == parity) {
            parity_result = true;
        }

        if(!parity_result) {
            printf("Ошибка четности: ");
            for(int i=0; i<30; i++) {
                int bit = (word >> (31-i)) & 0x1;
                printf("%d", bit);
            }
            printf("      ");
            printf("Ошибка четности: ");
            for(int i=0; i<6; i++) {
                int bit = (parity >> (5-i)) & 0x1;
                printf("%d", bit);
            }
            printf("\n");
            continue;
        }

        parity_result = false;

        bit29 = (parity & 0x02) >> 1;
        bit30 = parity & 0x01;

        message[0] = word;

        printf("\nНомер кадра равен: %d\n", (word >> 18) & 0x0000003f);
        printf("Первый кадр: ");
        for(int i=0; i<30; i++) {
            int bit = (word >> (31-i)) & 0x1;
            printf("%d", bit);
        }
        printf("      ");

        cut1 = preamble.preamble_place / 6;
        cut2 = 6 - (preamble.preamble_place - cut1 * 6);
        
        printf("Четность: ");
        for(int i=0; i<6; i++) {
            int bit = (parity >> (5-i)) & 0x1;
            printf("%d", bit);
        }

        printf("   bit29: %d, bit30: %d", bit29, bit30);
        printf("\n");


        for(int i = 0; i < (cut1 + 1); i++) {
            if (!ReadFile(PORT4, buffer, sizeof(buffer), &Readed, NULL)) { // Чтение начало
            printf("Ошибка чтения данных из файла!\n");
            Sleep(10000);
            return 1;
        }
        for(int i = 0; i < 8; i++) {
            uint8_t bit = (buffer >> i) & 0x01;
            if(bit == 1) {
                buffer_roll |= 0x01 << (7 - i);
            }
            bit = 0;
        }
        potok <<= 6;
        potok += (buffer_roll >> 2) & 0x3f;  
        buffer_roll = 0;
        buffer = 0; 
        CloseHandle(PORT4); // Чтение конец
        }

        word = (potok >> (cut2 + 2)) & 0xffffffff;

        if(bit30 == 1) {
            word = (~word & 0xffffff00) | word & 0x000000ff;
        }

        parity = ParityCreate(((word >> 2) & 0x3fffffff), bit30, bit29);

        if(((word >> 2) & 0x0000003f) == parity) {
            parity_result = true;
        }

        message[1] = word;
        Z1count = (word >> 19) & 0x1fff;
        *Z1 = &Z1count;

        bit29 = (parity & 0x02) >> 1;
        bit30 = parity & 0x01;

        chislo_kadrov = (message[1] >> 11) & 0x1f;
    
        printf("Количество кадров %d: ", ((message[1] >> 11) & 0x1f));
        printf("\n");

        printf("Второй кадр: ");
        for(int i=0; i<30; i++) {
            int bit = (word >> (31-i)) & 0x1;
            printf("%d", bit);
        }
        printf("      ");
        printf("Четность: ");
        for(int i=0; i<6; i++) {
            int bit = (parity >> (5-i)) & 0x1;
            printf("%d", bit);
        }
        printf("   bit29: %d, bit30: %d", bit29, bit30);
        printf("\n");

        for(int i = 0; i < chislo_kadrov; i++) {
            for(int j = 0; j < 5; j++) {
                if (!ReadFile(PORT4, buffer, sizeof(buffer), &Readed, NULL)) { // Чтение начало
                    printf("Ошибка чтения данных из файла!\n");
                    Sleep(10000);
                    return 1;
                }
                for(int i = 0; i < 8; i++) {
                    uint8_t bit = (buffer >> i) & 0x01;
                    if(bit == 1) {
                        buffer_roll |= 0x01 << (7 - i);
                    }
                    bit = 0;
                }
                potok <<= 6;
                potok += (buffer_roll >> 2) & 0x3f;  
                buffer_roll = 0;
                buffer = 0; 
                CloseHandle(PORT4); // Чтение конец
            }

            
            word = (potok >> (cut2 + 2)) & 0xffffffff;
            if(bit30 == 1) {
                word = (~word & 0xffffff00) | word & 0x000000ff;
            }
            parity = ParityCreate(((word >> 2) & 0x3fffffff), bit30, bit29);
            bit29 = (parity & 0x02) >> 1;
            bit30 = parity & 0x01;
            message[i+2] = word;

            for(int i=0; i<30; i++) {
            int bit = (word >> (31-i)) & 0x1;
            printf("%d", bit);
            }
            printf("      ");
            printf("Четность: ");
            for(int i=0; i<6; i++) {
                int bit = (parity >> (5-i)) & 0x1;
                printf("%d", bit);
            }
            printf("   bit29: %d, bit30: %d", bit29, bit30);
            printf("\n");

        }
        
        message[1] = SequencesChanger(message[1]);

        for(int i = 0; i < (chislo_kadrov + 2); i++) {
            parity = ParityCreate(((message[i] >> 2) & 0x3fffffff), *bit30, *bit29);
            *bit29 = (parity >> 1) & 0x01;
            *bit30 = parity & 0x01;
            message[i] = (message[i] & 0xffffff00) | ((parity << 2) & 0xfc);
        }

        if (!WriteFile(PORT1, message, sizeof(message), NULL, NULL)) {
            printf("Error writing to COM1 port\n");
            CloseHandle(PORT1);
            return 1;
        }

    }
    CloseHandle(PORT4);
    CloseHandle(PORT1);
}

void* secondCOM() {

    HANDLE PORT5 = CreateFile("COM1", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (PORT5 == INVALID_HANDLE_VALUE) {
        printf("Error opening COM1\n");
        Sleep(10000);
        return;
    }

    if (!GetCommState(PORT5, &dcb)) {
        printf("Error getting COM1 port state\n");
        CloseHandle(PORT5);
        Sleep(10000);
        return;
    }

    HANDLE PORT1 = CreateFile("COM1", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (PORT1 == INVALID_HANDLE_VALUE) {
        printf("Error opening COM1\n");
        return 1;
    }

    if (!GetCommState(PORT1, &dcb)) {
        printf("Error getting COM1 port state\n");
        CloseHandle(PORT1);
        return 1;
    }

    uint64_t potok;
    uint32_t word;
    uint32_t message[31];
    uint16_t Z2count;
    uint8_t buffer, buffer_roll;
    uint8_t bit29, bit30, parity;
    uint8_t cut1, cut2, chislo_kadrov;
    bool parity_result;
    DWORD Readed;
    bit29 = 1;
    bit30 = 1;

    while(1) {

        parity_result = false;
        if (!ReadFile(PORT5, buffer, sizeof(buffer), &Readed, NULL)) { // Чтение начало
            printf("Ошибка чтения данных из файла!\n");
            Sleep(10000);
            return 1;
        }
        for(int i = 0; i < 8; i++) {
            uint8_t bit = (buffer >> i) & 0x01;
            if(bit == 1) {
                buffer_roll |= 0x01 << (7 - i);
            }
            bit = 0;
        }
        potok <<= 6;
        potok += (buffer_roll >> 2) & 0x3f;  
        buffer_roll = 0;
        buffer = 0; 
        CloseHandle(PORT5); // Чтение конец

        Preamble preamble = PreambleSearch(potok, bit30, bit29);

        if(!preamble.preamble_result) {
            continue;
        }

        word = (potok >> (32 - preamble.preamble_place)) & 0xffffffff;

        if(bit30 == 1) {
            word = (~word & 0xffffff00) | word & 0x000000ff;
        }

        parity = ParityCreate(((word >> 2) & 0x3fffffff), bit30, bit29);

        if(((word >> 2) & 0x0000003f) == parity) {
            parity_result = true;
        }

        if(!parity_result) {
            printf("Ошибка четности: ");
            for(int i=0; i<30; i++) {
                int bit = (word >> (31-i)) & 0x1;
                printf("%d", bit);
            }
            printf("      ");
            printf("Ошибка четности: ");
            for(int i=0; i<6; i++) {
                int bit = (parity >> (5-i)) & 0x1;
                printf("%d", bit);
            }
            printf("\n");
            continue;
        }

        parity_result = false;

        bit29 = (parity & 0x02) >> 1;
        bit30 = parity & 0x01;

        message[0] = word;

        printf("\nНомер кадра равен: %d\n", (word >> 18) & 0x0000003f);
        printf("Первый кадр: ");
        for(int i=0; i<30; i++) {
            int bit = (word >> (31-i)) & 0x1;
            printf("%d", bit);
        }
        printf("      ");

        cut1 = preamble.preamble_place / 6;
        cut2 = 6 - (preamble.preamble_place - cut1 * 6);
        
        printf("Четность: ");
        for(int i=0; i<6; i++) {
            int bit = (parity >> (5-i)) & 0x1;
            printf("%d", bit);
        }

        printf("   bit29: %d, bit30: %d", bit29, bit30);
        printf("\n");


        for(int i = 0; i < (cut1 + 1); i++) {
            if (!ReadFile(PORT5, buffer, sizeof(buffer), &Readed, NULL)) { // Чтение начало
            printf("Ошибка чтения данных из файла!\n");
            Sleep(10000);
            return 1;
        }
        for(int i = 0; i < 8; i++) {
            uint8_t bit = (buffer >> i) & 0x01;
            if(bit == 1) {
                buffer_roll |= 0x01 << (7 - i);
            }
            bit = 0;
        }
        potok <<= 6;
        potok += (buffer_roll >> 2) & 0x3f;  
        buffer_roll = 0;
        buffer = 0; 
        CloseHandle(PORT5); // Чтение конец
        }

        word = (potok >> (cut2 + 2)) & 0xffffffff;

        if(bit30 == 1) {
            word = (~word & 0xffffff00) | word & 0x000000ff;
        }

        parity = ParityCreate(((word >> 2) & 0x3fffffff), bit30, bit29);

        if(((word >> 2) & 0x0000003f) == parity) {
            parity_result = true;
        }

        message[1] = word;

        Z2count = (word >> 19) & 0x1fff;
        *Z2 = &Z2count;


        bit29 = (parity & 0x02) >> 1;
        bit30 = parity & 0x01;

        chislo_kadrov = (message[1] >> 11) & 0x1f;
    
        printf("Количество кадров %d: ", ((message[1] >> 11) & 0x1f));
        printf("\n");

        printf("Второй кадр: ");
        for(int i=0; i<30; i++) {
            int bit = (word >> (31-i)) & 0x1;
            printf("%d", bit);
        }
        printf("      ");
        printf("Четность: ");
        for(int i=0; i<6; i++) {
            int bit = (parity >> (5-i)) & 0x1;
            printf("%d", bit);
        }
        printf("   bit29: %d, bit30: %d", bit29, bit30);
        printf("\n");

        for(int i = 0; i < chislo_kadrov; i++) {
            for(int j = 0; j < 5; j++) {
                if (!ReadFile(PORT5, buffer, sizeof(buffer), &Readed, NULL)) { // Чтение начало
                    printf("Ошибка чтения данных из файла!\n");
                    Sleep(10000);
                    return 1;
                }
                for(int i = 0; i < 8; i++) {
                    uint8_t bit = (buffer >> i) & 0x01;
                    if(bit == 1) {
                        buffer_roll |= 0x01 << (7 - i);
                    }
                    bit = 0;
                }
                potok <<= 6;
                potok += (buffer_roll >> 2) & 0x3f;  
                buffer_roll = 0;
                buffer = 0; 
                CloseHandle(PORT5); // Чтение конец
            }

            
            word = (potok >> (cut2 + 2)) & 0xffffffff;
            if(bit30 == 1) {
                word = (~word & 0xffffff00) | word & 0x000000ff;
            }
            parity = ParityCreate(((word >> 2) & 0x3fffffff), bit30, bit29);
            bit29 = (parity & 0x02) >> 1;
            bit30 = parity & 0x01;
            message[i+2] = word;

            for(int i=0; i<30; i++) {
            int bit = (word >> (31-i)) & 0x1;
            printf("%d", bit);
            }
            printf("      ");
            printf("Четность: ");
            for(int i=0; i<6; i++) {
                int bit = (parity >> (5-i)) & 0x1;
                printf("%d", bit);
            }
            printf("   bit29: %d, bit30: %d", bit29, bit30);
            printf("\n");

        }
        
        message[1] = SequencesChanger(message[1]);

        for(int i = 0; i < (chislo_kadrov + 2); i++) {
            parity = ParityCreate(((message[i] >> 2) & 0x3fffffff), *bit30, *bit29);
            *bit29 = (parity >> 1) & 0x01;
            *bit30 = parity & 0x01;
            message[i] = (message[i] & 0xffffff00) | ((parity << 2) & 0xfc);
        }

        if (!WriteFile(PORT1, message, sizeof(message), NULL, NULL)) {
            printf("Error writing to COM1 port\n");
            CloseHandle(PORT1);
            return 1;
        }
        
    }
    CloseHandle(PORT5);
    CloseHandle(PORT1);
}

int main(){

    pthread_t thread1, thread2;

    // Создание первого потока
    if (pthread_create(&thread1, NULL, firstCOM, NULL) != 0) {
        printf("Не удалось создать поток для первого порта\n");
        return 1;
    }

    // Создание второго потока
    if (pthread_create(&thread2, NULL, secondCOM, NULL) != 0) {
        printf("Не удалось создать поток для второго порта\n");
        return 1;
    }

    // Ожидание завершения обоих потоков
    if (pthread_join(thread1, NULL) != 0) {
        printf("Ошибка запуска первого потока\n");
        return 1;
    }

    if (pthread_join(thread2, NULL) != 0) {
        printf("Ошибка запуска второго потока\n");
        return 1;
    }

    printf("Процесс завершен\n");
    
    return 0;
}
