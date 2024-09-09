#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <Windows.h>

pthread_mutex_t lock1, lock2, lock3, lock4;
uint8_t *word1bit29, *word1bit30, *word2bit29, *word2bit30;
uint16_t *Z1, *Z2;
uint32_t *seq;

void initialize() {
    word1bit29 = (uint8_t *)malloc(sizeof(uint8_t));
    if (!word1bit29) {
        perror("Failed to allocate memory for wordbit29");
        exit(1);
    }
    
    word1bit30 = (uint8_t *)malloc(sizeof(uint8_t));
    if (!word1bit30) {
        perror("Failed to allocate memory for wordbit30");
        exit(1);
    }

    word2bit29 = (uint8_t *)malloc(sizeof(uint8_t));
    if (!word2bit29) {
        perror("Failed to allocate memory for wordbit29");
        exit(1);
    }
    
    word2bit30 = (uint8_t *)malloc(sizeof(uint8_t));
    if (!word2bit30) {
        perror("Failed to allocate memory for wordbit30");
        exit(1);
    }
    
    Z1 = (uint16_t *)malloc(sizeof(uint16_t));
    if (!Z1) {
        perror("Failed to allocate memory for Z1");
        exit(1);
    }
    
    Z2 = (uint16_t *)malloc(sizeof(uint16_t));
    if (!Z2) {
        perror("Failed to allocate memory for Z2");
        exit(1);
    }
    
    seq = (uint32_t *)malloc(sizeof(uint32_t));
    if (!seq) {
        perror("Failed to allocate memory for seq");
        exit(1);
    }
    
    *seq = 0;
    *word1bit29 = 1;
    *word1bit30 = 1;
}


// void wordbitsync(uint8_t N) {

//     pthread_mutex_lock(&lock4);

//     if(N == 1) {
//         *word2bit29 = *word1bit29;
//         *word2bit30 = *word1bit30;
//     } else if (N == 2) {
//         *word1bit29 = *word2bit29;
//         *word1bit30 = *word2bit30;
//     }
    

//     pthread_mutex_unlock(&lock4);

// }

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

    pthread_mutex_lock(&lock3);

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

    pthread_mutex_unlock(&lock3);

    return cod;
}

uint32_t SequencesChanger(uint32_t word) {

    pthread_mutex_lock(&lock1);
    uint32_t seqe = *seq;
    word = (word & 0xfff8fffc) | ((seqe << 16) & 0x00070000);
    
    if(seqe == 7) {
        *seq = 0;
    } else {
        *seq = *seq + 1;
    }

    pthread_mutex_unlock(&lock1);
    
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

void COMWriter(uint32_t mes[], uint8_t kadry) {

    pthread_mutex_lock(&lock2);
//    uint32_t buffer_write;
    uint8_t buffer, buffer_roll, bit29, bit30, parity;
    DWORD bytesWritten;

    bit29 = *word1bit29;
    bit30 = *word1bit30;

    for(int i = 0; i < (kadry + 2); i++) {

        parity = ParityCreate(((mes[i] >> 2) & 0x3fffffff), bit30, bit29);

        mes[i] = (mes[i] & 0xffffff00) | (parity << 2);

        if(bit30 == 1) {
            mes[i] = (~mes[i] & 0xffffff00) | mes[i] & 0x000000ff;
        }

        bit29 = (parity & 0x02) >> 1;
        bit30 = parity & 0x01;

        mes[i] >>= 2;
        for(int j = 0; j < 5; j++) {
            buffer_roll = 0;
            buffer = (((mes[i] >> 6 * (4 - j)) & 0x3f) << 2);
            for(int g = 0; g < 8; g++) {
                uint8_t bit = (buffer >> g) & 0x01;
                if(bit == 1) {
                    buffer_roll |= 0x01 << (7 - g);
                }
                bit = 0;
            }
            buffer_roll |= 0x40;
            HANDLE hSerial3 = CreateFile("COM3", GENERIC_WRITE | GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
            WriteFile(hSerial3, &buffer_roll, sizeof(uint8_t), &bytesWritten, NULL);
            CloseHandle(hSerial3);
        }

    }

    *word1bit29 = bit29;
    *word1bit30 = bit30;
    *word2bit29 = bit29;
    *word2bit30 = bit30;
    

    pthread_mutex_unlock(&lock2);

}

void* firstCOM() {

    HANDLE hSerial1 = CreateFile("COM1", GENERIC_WRITE | GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);

    uint64_t potok;
    uint32_t word;
    uint32_t message[31];
    uint16_t Z1count;
    uint8_t buffer, buffer_roll;
    uint8_t bit29, bit30, startbit29, startbit30, parity;
    uint8_t cut1, cut2, chislo_kadrov;
    bool parity_result;
    DWORD bytesRead;
    bit29 = 1;
    bit30 = 1;
    startbit29 = bit29;
    startbit30 = bit30;
    uint16_t count;

    while(1) {
        count++;
        parity_result = false;
        // printf("Первая контрольная точка первого потока\n");
        ReadFile(hSerial1, &buffer, sizeof(buffer), &bytesRead, NULL); // Чтение начало
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
        buffer = 0; // Чтение конец

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
            continue;
        }

        parity_result = false;

        bit29 = (parity & 0x02) >> 1;
        bit30 = parity & 0x01;

        message[0] = word;

        cut1 = preamble.preamble_place / 6;
        cut2 = 6 - (preamble.preamble_place - cut1 * 6);
        
        for(int i = 0; i < (cut1 + 1); i++) {
        ReadFile(hSerial1, &buffer, sizeof(buffer), &bytesRead, NULL); // Чтение начало
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
        buffer = 0; // Чтение конец
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

        bit29 = (parity & 0x02) >> 1;
        bit30 = parity & 0x01;

        chislo_kadrov = (message[1] >> 11) & 0x1f;
        
        for(int i = 0; i < chislo_kadrov; i++) {
            for(int j = 0; j < 5; j++) {
                ReadFile(hSerial1, &buffer, sizeof(buffer), &bytesRead, NULL); // Чтение начало
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
                buffer = 0; // Чтение конец
            }
            
            
            word = (potok >> (cut2 + 2)) & 0xffffffff;
            if(bit30 == 1) {
                word = (~word & 0xffffff00) | word & 0x000000ff;
            }
            parity = ParityCreate(((word >> 2) & 0x3fffffff), bit30, bit29);
            bit29 = (parity & 0x02) >> 1;
            bit30 = parity & 0x01;
            message[i+2] = word;
        }
        
        message[1] = SequencesChanger(message[1]);
        // startbit29 = *word1bit29;
        // startbit30 = *word1bit30;

        // for(int i = 0; i < (chislo_kadrov + 2); i++) {
        //     parity = ParityCreate(((message[i] >> 2) & 0x3fffffff), startbit30, startbit29);
        //     startbit29 = (parity >> 1) & 0x01;
        //     startbit30 = parity & 0x01;
        //     message[i] = (message[i] & 0xffffff00) | ((parity << 2) & 0xfc);
        // }

        // *word1bit29 = startbit29;
        // *word1bit30 = startbit30;
        uint8_t Np = 1;
        Sleep(50);
        // wordbitsync(Np);
        
        COMWriter(message, chislo_kadrov);
        
        
        

    }
    CloseHandle(hSerial1);
}

void* secondCOM() {
    
    HANDLE hSerial2 = CreateFile("COM2", GENERIC_WRITE | GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);

    uint64_t potok;
    uint32_t word;
    uint32_t message[31];
    uint16_t Z2count;
    uint8_t buffer, buffer_roll;
    uint8_t bit29, bit30, startbit29, startbit30, parity;
    uint8_t cut1, cut2, chislo_kadrov;
    bool parity_result;
    DWORD bytesRead;
    bit29 = 1;
    bit30 = 1;
    startbit29 = bit29;
    startbit30 = bit30;
    uint16_t count;

    while(1) {
        count++;
        parity_result = false;
        ReadFile(hSerial2, &buffer, sizeof(buffer), &bytesRead, NULL); // Чтение начало
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
        buffer = 0; // Чтение конец

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
            continue;
        }

        parity_result = false;

        bit29 = (parity & 0x02) >> 1;
        bit30 = parity & 0x01;

        message[0] = word;

        cut1 = preamble.preamble_place / 6;
        cut2 = 6 - (preamble.preamble_place - cut1 * 6);
        
        // printf("Вторая контрольная точка второго потока\n");
        for(int i = 0; i < (cut1 + 1); i++) {
        ReadFile(hSerial2, &buffer, sizeof(buffer), &bytesRead, NULL); // Чтение начало
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
        buffer = 0; // Чтение конец
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
        
        // *Z2 = Z2count;

        bit29 = (parity & 0x02) >> 1;
        bit30 = parity & 0x01;

        chislo_kadrov = (message[1] >> 11) & 0x1f;
        
        for(int i = 0; i < chislo_kadrov; i++) {
            for(int j = 0; j < 5; j++) {
                ReadFile(hSerial2, &buffer, sizeof(buffer), &bytesRead, NULL); // Чтение начало
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
                buffer = 0; // Чтение конец
            }
            
            
            word = (potok >> (cut2 + 2)) & 0xffffffff;
            if(bit30 == 1) {
                word = (~word & 0xffffff00) | word & 0x000000ff;
            }
            parity = ParityCreate(((word >> 2) & 0x3fffffff), bit30, bit29);
            bit29 = (parity & 0x02) >> 1;
            bit30 = parity & 0x01;
            message[i+2] = word;
        }
        
        message[1] = SequencesChanger(message[1]);
        // printf("Третья контрольная точка второго потока\n");
        // startbit29 = *word2bit29;
        // startbit30 = *word2bit30;

        // for(int i = 0; i < (chislo_kadrov + 2); i++) {
        //     parity = ParityCreate(((message[i] >> 2) & 0x3fffffff), startbit30, startbit29);
        //     startbit29 = (parity >> 1) & 0x01;
        //     startbit30 = parity & 0x01;
        //     message[i] = (message[i] & 0xffffff00) | ((parity << 2) & 0xfc);
        // }

        uint8_t Np = 2;
        Sleep(50);
        // wordbitsync(Np);
        
        COMWriter(message, chislo_kadrov);

        
        
    }
    CloseHandle(hSerial2);
}

int main(){

    pthread_t thread1, thread2;
    
    initialize();

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
