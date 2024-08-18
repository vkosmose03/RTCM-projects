#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

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

int main() {

    FILE *file = fopen("1.cor", "rb");
    if (file == NULL) {
        printf("Не удалось открыть файл\n");
    }
    
    uint64_t potok;
    uint32_t word;
    uint32_t message[31];
    uint8_t buffer, buffer_roll;
    uint8_t bit29, bit30, parity;
    uint8_t cut1, cut2, chislo_kadrov;
    bool parity_result;
    bit29 = 1;
    bit30 = 1;

    while(1) {

        parity_result = false;
        fread(&buffer, sizeof(buffer), 1, file); // Чтение начало
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

        // for(int i=0; i<64; i++) {
        //     int bit = (potok >> (63-i)) & 0x1;
        //     printf("%d", bit);
        // }
        // printf("\n");

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
            fread(&buffer, sizeof(buffer), 1, file); // Чтение начало
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
                fread(&buffer, sizeof(buffer), 1, file); // Чтение начало
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

        getchar();
    }



    fclose(file);
    getchar();
    return 0;
}