#include "./my_board.h"


extern u8 USART_Recv_Flag;
extern u8 USART_RecvBuf[USART_RECVBUF_LENGTH];
extern u8 USART1_RecvBuf_Length;
extern u8 WAKEUP_SOURCE;
extern QS808_Rec_Buf_type QS808_Rec_Buf;

float Battery_quantity;
u8 SLEEP_MAX = 30;

int main(void) {
	delay_init();			// 【系统时钟】初始化
	RTC_Init();				// 【RTC时钟】初始化
	TSM12_Init();			// 【触摸按键芯片】初始化
	BLE_init();				// 【蓝牙】初始化
	debug_usart_init();		// 【串口】初始化
	power_ctrl_init();		// 【电源控制】初始化
	Power_ctrl_on();		// 打开电源控制
	RC522_Init();			// 【射频卡芯片】初始化
	TIM3_Int_Init(99,7199);	// 定时器3，测试函数运行时间用,10K频率计数,每10ms一个中断
	QS808_Init();			// 【指纹采集头】初始化
	Gate_Init();			// 【门锁机械控制】初始化
	OLED_Init();			// 【OLED】初始化
	delay_ms(100);
	VCC_Adc_Init();			// 【ADC】通道初始化
	UT588C_init();			// 【语音芯片】初始化
	// QS808_CMD_DEL_ALL();	// 删除全部指纹
	
	u8 work_flag;
	u16 temp_cmdid,temp_userid,temp_return,sleep_count;
	u32 temp_RFCARD_ID;

	sleep_count = 0;
	show_clock_close_big();
	while(1) {
		// 睡眠计数++
		sleep_count++;
		
		// 显示主界面
		if (work_flag==1) {
			work_flag = 0;
			show_clock_close_big();
		}

		// 判断是否该进入睡眠模式
		if (sleep_count>=SLEEP_MAX && QS808_Rec_Buf.Trans_state==reset) {
			sleep_count = 0;
			QS808_Rec_Buf_refresh();
			Disp_sentence(48,2,"休眠",1);
			delay_ms(500);
			PWR_Standby_Mode();

			// 唤醒之后从这里开始执行程序，先执行一次指纹扫描，加速开锁进程
			show_clock_close_big();
			// 如果是指纹头唤醒
			if(WAKEUP_SOURCE==0) {
				// 如果检测到有手指按下，就开始检测指纹正确性，准备开门
				if (QS808_CMD_FINGER_DETECT()==ERR_FINGER_DETECT) {
					sleep_count = SLEEP_MAX;
					temp_return = Confirm_Finger();
					if (temp_return==ERROR_CODE_SUCCESS) {
						show_clock_open_big();
						SPEAK_OPEN_THE_DOOR();
						Gate_Unlock();
					}
					else {
						Disp_sentence(28,2,"验证失败",1);
						SPEAK_OPEN_THE_DOOR_FAIL();
						delay_ms(1000);	// 验证失败的显示时间
					}
				}
			}
		}

		// 如果接收到了数据传入，说明手机端发来了信息，可能要进行信息录入或者一键开锁
		if ( Usart_RecvOrder(USART1)==SYS_RECV_ORDER ) {
			// 根据 temp_cmdid 来进行分支判断
			temp_cmdid = RecvBuf2Cmdid();
			switch( temp_cmdid ) {
				/************************* 接收到【添加指纹】指令 *************************/
				case CMDID_ADD_FINGER:
					sleep_count = 0;
					SPEAK_DUDUDU();
					if(Add_Finger(&temp_userid) == ERROR_CODE_SUCCESS)
						Usart_SendFinger_ADD_Success(USART1, 3, temp_userid);
					// 延时一段时间，防止直接进入指纹解锁步骤
					delay_ms(1000);
					break;


				/************************* 接收到【删除指纹】指令 *************************/
				case CMDID_DEL_FINGER:
					sleep_count = 0;
					SPEAK_DUDUDU();
					temp_userid = RecvBuf2Userid();
					if (Delete_Finger(temp_userid) == ERROR_CODE_SUCCESS)
						Usart_SendFinger_DEL_Success(USART1);
					else
						Usart_SendFinger_DEL_Error(USART1);
					break;


				/************************ 接收到【添加射频卡】指令 ************************/
				case CMDID_ADD_RFCARD:
					sleep_count = 0;
					SPEAK_DUDUDU();
					temp_return = Add_RFCard(&temp_userid);
					if(temp_return== ERROR_CODE_SUCCESS)
						Usart_SendRFCard_ADD_Success(USART1, temp_userid);
					else
						Usart_SendRFCard_ADD_Error(USART1, temp_return);
					// 延时一段时间，防止直接进入射频卡解锁步骤
					delay_ms(1000);
					break;


				/************************ 接收到【删除射频卡】指令 ************************/
				case CMDID_DEL_RFCARD:
					sleep_count = 0;
					SPEAK_DUDUDU();
					temp_userid = RecvBuf2Userid();
					if (Delete_RFCard(temp_userid) == ERROR_CODE_SUCCESS)
						Usart_SendRFCard_DEL_Success(USART1);
					else
						Usart_SendRFCard_DEL_Error(USART1);
					break;


				/************************* 接收到【添加密码】指令 *************************/
				case CMDID_ADD_PASSWORD:
					sleep_count = 0;
					SPEAK_DUDUDU();
					temp_return = Add_Password(&temp_userid);
					if(temp_return== ERROR_CODE_SUCCESS)
						Usart_SendPassword_ADD_Success(USART1, temp_userid);
					else
						Usart_SendPassword_ADD_Error(USART1, temp_return);
					break;


				/************************* 接收到【删除密码】指令 *************************/
				case CMDID_DEL_PASSWORD:
					sleep_count = 0;
					SPEAK_DUDUDU();
					temp_userid = RecvBuf2Userid();
					if (Delete_Password(temp_userid) == ERROR_CODE_SUCCESS)
						Usart_SendPassword_DEL_Success(USART1);
					else
						Usart_SendPassword_DEL_Error(USART1);
					break;
				/************************************************************************/
				default:
					break;
			}
		}
		// 如果没有接收到手机端发来的信息，那就时刻准备开锁
		else {
			// 如果检测到有手指按下，就开始检测指纹正确性，准备开门
			if (QS808_CMD_FINGER_DETECT()==ERR_FINGER_DETECT) {
				sleep_count = 0;
				work_flag = 1;
				temp_return = Confirm_Finger();
				if (temp_return==ERROR_CODE_SUCCESS) {
					show_clock_open_big();
					SPEAK_OPEN_THE_DOOR();
					Gate_Unlock();
				}
				else {
					Disp_sentence(28,2,"验证失败",1);
					SPEAK_OPEN_THE_DOOR_FAIL();
				}
				// 防止短时间内再次进入指纹检测
				delay_ms(1000);
			}

			// 如果检测到有射频卡靠近，就开始检测射频卡的正确性，准备开门
			if (RFCard_test(&temp_RFCARD_ID) == RFCARD_DETECED) {
				sleep_count = 0;
				work_flag = 1;
				temp_return = Confirm_RFCard(temp_RFCARD_ID);
				if (temp_return==ERROR_CODE_SUCCESS) {
					show_clock_open_big();
					SPEAK_OPEN_THE_DOOR();
					Gate_Unlock();
				}
				else {
					Disp_sentence(28,2,"验证失败",1);
					SPEAK_OPEN_THE_DOOR_FAIL();
				}
				// 验证失败显示时间
				delay_ms(1000);
			}

			// 如果检测到有按键按下，就进入密码解锁界面，准备开门
			if (TMS12_ReadOnKey() != KEY_NULL) {
				sleep_count = 0;
				work_flag = 1;
				u8 password_buf[LENGTH_KEY_BUF], buf_length=0, last_press, temp;
				SPEAK_DUDUDU();
				OLED_CLS();

				// 密码输入界面
				Battery_quantity = Get_Battery();
				if (Battery_quantity<3.7)		OLED_Show_Power(0);
				else if (Battery_quantity<4.2)	OLED_Show_Power(1);
				else if (Battery_quantity<4.7)	OLED_Show_Power(2);
				else if (Battery_quantity<5.2)	OLED_Show_Power(3);
				else 							OLED_Show_Power(4);
				Disp_sentence(24,2,"请输入密码",0);

				Create_NewPasswordBuf(password_buf);
				while(1) {
					// 更新按键缓冲区X
					temp = Update_KeyBuf(password_buf, &buf_length, &last_press);
					// 如果现在正在输入密码，就响一下
					if (temp==PASSWORD_CODE_INPUTTING) {
						SPEAK_DUDUDU();
						Interface_Password(buf_length);
					}
					// 如果密码输入超时或者输入退出，就直接break
					else if (temp==PASSWORD_CODE_TIMEOUT || temp==PASSWORD_CODE_QUIT) {
						SPEAK_DUDUDU();
						OLED_CLS();
						break;
					}
					// 如果密码输入完成，就开始验证密码的准确性
					else if (temp==PASSWORD_CODE_COMPLETE) {
						SPEAK_DUDUDU();
						if (Confirm_Password(password_buf, buf_length)==ERROR_CODE_SUCCESS) {
							show_clock_open_big();
							SPEAK_OPEN_THE_DOOR();
							Gate_Unlock();
						}
						else {
							Disp_sentence(28,2,"验证失败",1);
							SPEAK_OPEN_THE_DOOR_FAIL();
						}
						// 留给界面显示
						delay_ms(1000);
						break;
					}
					// 密码长度不足6位
					else if (temp==PASSWORD_CODE_SHORT) {
						SPEAK_DUDUDU();
						Disp_sentence(28,2,"验证失败",1);
						SPEAK_OPEN_THE_DOOR_FAIL();
						// 留给界面显示
						delay_ms(1000);
						break;
					}
				}
			}
		}
	}
}

// // 两数相加
// int* twoSum(int* nums, int numsSize, int target) {
//     int min = 2147483647;
//     int i = 0;
//     for (i = 0; i < numsSize; i++) {
//         if (nums[i] < min)
//             min = nums[i];
//     }
//     int max = target - min;
//     int len = max - min + 1;   // 确定hash长度
//     int *table = (int*)malloc(len*sizeof(int));
//     int *indice = (int*)malloc(2*sizeof(int));


//     for (i = 0; i < len; i++) {
//         table[i] = -1;         // hash初值，取一个不可能出现的数值，也就是小于0就行
//     }
//     for (i = 0; i < numsSize; i++) {
//         if (nums[i]-min < len) {
//             if (table[target-nums[i]-min] != -1) {        //满足相加为target
//                 indice[0] = table[target-nums[i] - min];
//                 indice[1] = i;
//                 return indice;
//             }
//             table[nums[i]-min] = i;
//         }
//     }
//     free(table);
//     return indice;
// }
