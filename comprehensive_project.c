#include <reg51.h>
#include <intrins.h>
#define FOSC 11059200L  

void DelayUs(unsigned int us) {
    #if (FOSC <= 12000000L)
        while(us--) { _nop_(); _nop_(); }
    #else
        while(us--);
    #endif
}

void DelayMs(unsigned int ms) {
    unsigned int i, j;
    for(i = ms; i > 0; i--) {
        #if (FOSC == 11059200L)
            for(j = 114; j > 0; j--);
        #else
            for(j = 120; j > 0; j--);
        #endif
    }
}
sbit LCD_RS = P2^6;
sbit LCD_RW = P2^5; 
sbit LCD_E  = P2^7;
#define LCD_DATA P0

void Buzzer_Beep(void) {
    unsigned int i;
    for(i = 0; i < 75; i++) {
        LCD_RW = !LCD_RW; 
        DelayUs(200);     
    }
    LCD_RW = 0; 
}

void LCD_WaitBusy(void) {
    LCD_RW = 0;     
    DelayUs(500);   
}

void LCD_WriteCmd(unsigned char cmd) {
    LCD_WaitBusy(); LCD_RS = 0; LCD_DATA = cmd;
    _nop_(); LCD_E = 1; DelayUs(5); LCD_E = 0;
    if (cmd == 0x01 || cmd == 0x02) DelayMs(3); else DelayMs(1);
}

void LCD_WriteData(unsigned char dat) {
    LCD_WaitBusy(); LCD_RS = 1; LCD_DATA = dat;
    _nop_(); LCD_E = 1; DelayUs(5); LCD_E = 0; DelayUs(200); 
}

void LCD_Init(void) {
    DelayMs(40); LCD_RS = 0; LCD_RW = 0; LCD_E = 0; LCD_DATA = 0x30;
    LCD_E = 1; DelayUs(5); LCD_E = 0; DelayMs(6);
    LCD_E = 1; DelayUs(5); LCD_E = 0; DelayUs(250);
    LCD_E = 1; DelayUs(5); LCD_E = 0; DelayMs(2);
    LCD_WriteCmd(0x38); LCD_WriteCmd(0x0C); LCD_WriteCmd(0x06); LCD_WriteCmd(0x01); DelayMs(5);
}

void LCD_ClearRow(unsigned char row) {
    unsigned char i;
    LCD_WriteCmd((row == 0) ? 0x80 : 0xC0);
    for(i = 0; i < 16; i++) LCD_WriteData(' ');
    DelayMs(1);
}

void LCD_ShowString(unsigned char row, unsigned char col, char *str) {
    LCD_WriteCmd((row == 0) ? (0x80 + col) : (0xC0 + col));
    while(*str && (col++ < 16)) LCD_WriteData(*str++);
}

void LCD_ShowNum(unsigned char row, unsigned char col, long num) {
    char buf[11];
    unsigned char i = 0, count = 0;
    unsigned long temp;
    if (num < 0) { buf[i++] = '-'; temp = -num; } else temp = num;
    do { buf[i++] = (temp % 10) + '0'; temp /= 10; } while (temp > 0);
    LCD_WriteCmd((row == 0) ? (0x80 + col) : (0xC0 + col));
    if(buf[0] == '-') { LCD_WriteData('-'); count = 1; }
    while (i > count) LCD_WriteData(buf[--i]);
}
unsigned char Hour = 16, Min = 28, Sec = 0;    
unsigned int T0Count = 0; 
unsigned char TimeEditState = 0; 

void Timer0_Init(void) {
    TMOD &= 0xF0; TMOD |= 0x01; TL0 = 0x00; TH0 = 0x4C; TF0 = 0; TR0 = 1; ET0 = 1; EA = 1;      
}

void Timer0_Routine(void) interrupt 1 {
    TL0 = 0x00; TH0 = 0x4C; T0Count++;
    if (T0Count >= 20) { 
        T0Count = 0; Sec++;
        if (Sec >= 60) { Sec = 0; Min++; if (Min >= 60) { Min = 0; Hour++; if (Hour >= 24) Hour = 0; } }
    }
}

void Display_Clock(void) {
    char clock_str[15];
    LCD_ShowString(0, 0, "  Digital Clock ");
    clock_str[0]=' '; clock_str[1]=' '; clock_str[2]=' ';
    if (TimeEditState == 1 && (T0Count >= 10)) { clock_str[3]=' '; clock_str[4]=' '; }
    else { clock_str[3]='0'+Hour/10; clock_str[4]='0'+Hour%10; }
    clock_str[5]=':';
    if (TimeEditState == 2 && (T0Count >= 10)) { clock_str[6]=' '; clock_str[7]=' '; }
    else { clock_str[6]='0'+Min/10; clock_str[7]='0'+Min%10; }
    clock_str[8]=':';
    if (TimeEditState == 3 && (T0Count >= 10)) { clock_str[9]=' '; clock_str[10]=' '; }
    else { clock_str[9]='0'+Sec/10; clock_str[10]='0'+Sec%10; }
    clock_str[11]=' '; clock_str[12]=' '; clock_str[13]=' '; clock_str[14]='\0';
    LCD_ShowString(1, 0, clock_str);
}
unsigned char SystemMode = 0; 
long SavedResult = 0; bit Is_Result_Saved = 0;      
long LastCalcResult = 0;    

char CommonBuf[16] = "";     
unsigned char BufIdx = 0;
bit Has_Calculated = 0;
bit CalcErrorFlag = 0;       

void ResetCommonBuf(void) {
    unsigned char i;
    for(i = 0; i < 16; i++) CommonBuf[i] = 0;
    BufIdx = 0;
}
unsigned char GetPri(char op) {
    if (op == '+' || op == '-') return 1;
    if (op == '*' || op == '/') return 2;
    return 0; 
}

long EvaluateExpression(void) {
    long n_stack[4];  
    char o_stack[4];
    char n_top = -1, o_top = -1;
    unsigned char i = 0;
    CalcErrorFlag = 0; 

    while (CommonBuf[i] != '\0') {
        if (CommonBuf[i] >= '0' && CommonBuf[i] <= '9') {
            long val = 0;
            while (CommonBuf[i] >= '0' && CommonBuf[i] <= '9') {
                val = val * 10 + (CommonBuf[i] - '0'); i++;
            }
            if (n_top >= 3) { CalcErrorFlag = 1; return 0; }
            n_stack[++n_top] = val; continue;
        }
        if (CommonBuf[i] == '(') {
            if (o_top >= 3) { CalcErrorFlag = 1; return 0; }
            o_stack[++o_top] = '(';
        }
        else if (CommonBuf[i] == ')') {
            while (o_top >= 0 && o_stack[o_top] != '(') {
                long b, a; char op;
                if (n_top < 1) { CalcErrorFlag = 1; return 0; }
                b = n_stack[n_top--]; a = n_stack[n_top--]; op = o_stack[o_top--];
                if (op == '+') n_stack[++n_top] = a + b;
                else if (op == '-') n_stack[++n_top] = a - b;
                else if (op == '*') n_stack[++n_top] = a * b;
                else if (op == '/') { if (b == 0) { CalcErrorFlag = 1; return 0; } n_stack[++n_top] = a / b; }
            }
            if (o_top < 0) { CalcErrorFlag = 1; return 0; } 
            o_top--; 
        }
        else if (CommonBuf[i] == '+' || CommonBuf[i] == '-' || CommonBuf[i] == '*' || CommonBuf[i] == '/') {
            if (CommonBuf[i] == '-' && (i == 0 || CommonBuf[i-1] == '(')) {
                long val = 0; i++;
                while (CommonBuf[i] >= '0' && CommonBuf[i] <= '9') {
                    val = val * 10 + (CommonBuf[i] - '0'); i++;
                }
                if (n_top >= 3) { CalcErrorFlag = 1; return 0; }
                n_stack[++n_top] = -val; continue;
            }
            while (o_top >= 0 && o_stack[o_top] != '(' && GetPri(o_stack[o_top]) >= GetPri(CommonBuf[i])) {
                long b, a; char op;
                if (n_top < 1) { CalcErrorFlag = 1; return 0; }
                b = n_stack[n_top--]; a = n_stack[n_top--]; op = o_stack[o_top--];
                if (op == '+') n_stack[++n_top] = a + b;
                else if (op == '-') n_stack[++n_top] = a - b;
                else if (op == '*') n_stack[++n_top] = a * b;
                else if (op == '/') { if (b == 0) { CalcErrorFlag = 1; return 0; } n_stack[++n_top] = a / b; }
            }
            if (o_top >= 3) { CalcErrorFlag = 1; return 0; }
            o_stack[++o_top] = CommonBuf[i];
        }
        i++;
    }
    while (o_top >= 0) {
        long b, a; char op;
        if (o_stack[o_top] == '(') { CalcErrorFlag = 1; return 0; }
        if (n_top < 1) { CalcErrorFlag = 1; return 0; }
        b = n_stack[n_top--]; a = n_stack[n_top--]; op = o_stack[o_top--];
        if (op == '+') n_stack[++n_top] = a + b;
        else if (op == '-') n_stack[++n_top] = a - b;
        else if (op == '*') n_stack[++n_top] = a * b;
        else if (op == '/') { if (b == 0) { CalcErrorFlag = 1; return 0; } n_stack[++n_top] = a / b; }
    }
    if (n_top != 0) { CalcErrorFlag = 1; return 0; }
    return n_stack[0];
}
unsigned char GameCards[4];       
unsigned char GameGameState = 0;  

void Game_GenerateCards(void) {
    unsigned char i;
    unsigned int seed = TL0 + TH0 + T0Count; 
    for(i = 0; i < 4; i++) {
        seed = seed * 213 + 13;
        GameCards[i] = (seed % 10) + 1; 
    }
}

void ResetGame24(void) {
    Game_GenerateCards(); GameGameState = 0; ResetCommonBuf();
    LCD_ClearRow(0); LCD_ClearRow(1);
}

void Display_Game24(void) {
    if (GameGameState == 0) {
        char top_row[14];
        top_row[0] = ' '; top_row[1] = '0' + (GameCards[0] == 10 ? 1 : GameCards[0]);
        top_row[2] = GameCards[0] == 10 ? '0' : ' '; top_row[3] = ' ';
        top_row[4] = '0' + (GameCards[1] == 10 ? 1 : GameCards[1]);
        top_row[5] = GameCards[1] == 10 ? '0' : ' '; top_row[6] = ' ';
        top_row[7] = '0' + (GameCards[2] == 10 ? 1 : GameCards[2]);
        top_row[8] = GameCards[2] == 10 ? '0' : ' '; top_row[9] = ' ';
        top_row[10] = '0' + (GameCards[3] == 10 ? 1 : GameCards[3]);
        top_row[11] = GameCards[3] == 10 ? '0' : ' '; top_row[12] = '\0';
        LCD_ShowString(0, 0, top_row);
        if (BufIdx == 0) LCD_ShowString(1, 0, "_               ");
        else LCD_ShowString(1, 0, CommonBuf);
    } 
    else if (GameGameState == 1) {
        LCD_ShowString(0, 0, "  24 Point Game "); LCD_ShowString(1, 0, "    SUCCESS!    ");
    } 
    else if (GameGameState == 2) {
        LCD_ShowString(0, 0, "  24 Point Game "); LCD_ShowString(1, 0, "    WRONG!      ");
    }
}

void ProcessGame24(char key) {
    if (GameGameState != 0) { if (key == '=') { Buzzer_Beep(); ResetGame24(); } return; }
    if (key == 'C') { Buzzer_Beep(); ResetGame24(); return; }
    
    if ((key >= '0' && key <= '9') || key == '+' || key == '-' || key == '*' || key == '/' || key == '(' || key == ')') {
        if (BufIdx < 15) {
            Buzzer_Beep(); CommonBuf[BufIdx++] = key; CommonBuf[BufIdx] = '\0';
        }
    }
    else if (key == '=') {
        long res;
        Buzzer_Beep(); res = EvaluateExpression();
        if (!CalcErrorFlag && res == 24) GameGameState = 1; else GameGameState = 2;
        LCD_ClearRow(0); LCD_ClearRow(1);
    }
}
sbit K2 = P3^0; sbit K1 = P3^1; sbit K3 = P3^2; sbit K4 = P3^3;

void Independent_KeyScan(void) {
    if (K1 == 0) {
        DelayMs(10);
        if (K1 == 0) {
            Buzzer_Beep(); TimeEditState = 0; 
            if (SystemMode == 0) { SystemMode = 2; ResetGame24(); } 
            else { SystemMode = 0; ResetCommonBuf(); LCD_ClearRow(0); LCD_ClearRow(1); LCD_ShowString(0,0,"0"); }
            while(K1 == 0);
        }
    }
    if (K2 == 0) {
        DelayMs(10);
        if (K2 == 0) {
            Buzzer_Beep(); 
            if (SystemMode == 0 || SystemMode == 2) { SystemMode = 1; TimeEditState = 0; LCD_ClearRow(0); LCD_ClearRow(1); } 
            else { TimeEditState++; if (TimeEditState > 3) TimeEditState = 0; }
            while(K2 == 0);
        }
    }
    if (SystemMode == 0) { 
        if (K3 == 0) {
            DelayMs(10); if (K3 == 0) {
                Buzzer_Beep(); 
                SavedResult = LastCalcResult; Is_Result_Saved = 1;
                LCD_ShowString(0, 14, "M+"); 
                while(K3 == 0);
            }
        }
        if (K4 == 0) {
            DelayMs(10); if (K4 == 0) {
                Buzzer_Beep(); 
                if (Is_Result_Saved) { 
                    LCD_ClearRow(0); LCD_ShowString(0, 0, "Saved Value:"); 
                    LCD_ShowNum(1, 0, SavedResult); 
                    Has_Calculated = 1; 
                } else { 
                    LCD_ClearRow(0); LCD_ShowString(0, 0, "No Memory!"); 
                } 
                while(K4 == 0);
            }
        }
    } 
    else if (SystemMode == 2) { 
        if (K3 == 0) {
            DelayMs(10); if (K3 == 0) {
                ProcessGame24('(');
                while(K3 == 0);
            }
        }
        if (K4 == 0) {
            DelayMs(10); if (K4 == 0) {
                ProcessGame24(')');
                while(K4 == 0);
            }
        }
    }
    else if (SystemMode == 1) { 
        if (K3 == 0) {
            DelayMs(10); if (K3 == 0) {
                Buzzer_Beep();
                if (TimeEditState == 1) { Hour++; if (Hour >= 24) Hour = 0; } 
                else if (TimeEditState == 2) { Min++; if (Min >= 60) Min = 0; } 
                else if (TimeEditState == 3) Sec = 0; while(K3 == 0);
            }
        }
        if (K4 == 0) {
            DelayMs(10); if (K4 == 0) {
                Buzzer_Beep();
                if (TimeEditState == 1) { if (Hour == 0) Hour = 23; else Hour--; } 
                else if (TimeEditState == 2) { if (Min == 0) Min = 59; else Min--; } 
                else if (TimeEditState == 3) Sec = 0; while(K4 == 0);
            }
        }
    }
}
char code KeyMap[4][4] = {
    {'=', '*', '-', '+'}, {'/', '9', '6', '3'}, {'0', '8', '5', '2'}, {'C', '7', '4', '1'}
};
typedef enum { KEY_STATE_IDLE, KEY_STATE_DEBOUNCE, KEY_STATE_PRESSED, KEY_STATE_RELEASE_DEBOUNCE } KEY_STATUS_T;

char Matrix_KeyScan(void) {
    static KEY_STATUS_T key_state = KEY_STATE_IDLE;
    static unsigned char last_r = 0, last_c = 0;
    unsigned char r, c;
    unsigned char row_scan[] = {0xFE, 0xFD, 0xFB, 0xF7};
    char current_key = 0;
    for(r = 0; r < 4; r++) {
        P1 = row_scan[r]; _nop_(); _nop_(); DelayUs(20); 
        if((P1 & 0xF0) != 0xF0) {
            if((P1 & 0x10) == 0) c = 0; else if((P1 & 0x20) == 0) c = 1; else if((P1 & 0x40) == 0) c = 2; else if((P1 & 0x80) == 0) c = 3;
            current_key = KeyMap[r][c]; break;
        }
    }
    P1 = 0xFF; 
    switch(key_state) {
        case KEY_STATE_IDLE: if(current_key != 0) { last_r = r; last_c = c; key_state = KEY_STATE_DEBOUNCE; } break;
        case KEY_STATE_DEBOUNCE: if(current_key == KeyMap[last_r][last_c]) { key_state = KEY_STATE_PRESSED; return current_key; } else key_state = KEY_STATE_IDLE; break;
        case KEY_STATE_PRESSED: if(current_key == 0) key_state = KEY_STATE_RELEASE_DEBOUNCE; break;
        case KEY_STATE_RELEASE_DEBOUNCE: if(current_key == 0) key_state = KEY_STATE_IDLE; else key_state = KEY_STATE_PRESSED; break;
    }
    return 0;
}
void ProcessCalculator(char key) {
    if (key == 'C') { Buzzer_Beep(); ResetCommonBuf(); LCD_ClearRow(0); LCD_ClearRow(1); LCD_ShowString(0,0,"0"); return; }
    if (Has_Calculated && (key >= '0' && key <= '9')) { ResetCommonBuf(); Has_Calculated = 0; }
    
    if ((key >= '0' && key <= '9') || key == '+' || key == '-' || key == '*' || key == '/') {
        if (BufIdx < 15) {
            Buzzer_Beep(); CommonBuf[BufIdx++] = key; CommonBuf[BufIdx] = '\0';
            LCD_ShowString(0, 0, CommonBuf);
        }
    }
    else if (key == '=') {
        long result;
        Buzzer_Beep(); result = EvaluateExpression();
        if (CalcErrorFlag) { LCD_ClearRow(1); LCD_ShowString(1, 0, "Error!"); }
        else {
            LCD_ClearRow(1); LCD_ShowNum(1, 0, result);
            LastCalcResult = result; 
            Has_Calculated = 1;
        }
    }
}
void main(void) {
    LCD_RW = 0; Timer0_Init(); LCD_Init(); LCD_ShowString(0,0,"0");
    while(1) {
        Independent_KeyScan();
        if (SystemMode == 0) {
            char key_value = Matrix_KeyScan(); if(key_value != 0) ProcessCalculator(key_value); 
        } 
        else if (SystemMode == 1) { Display_Clock(); }
        else if (SystemMode == 2) {
            char key_value = Matrix_KeyScan(); Display_Game24(); if(key_value != 0) ProcessGame24(key_value);
        }
        DelayMs(10); 
    }
}
