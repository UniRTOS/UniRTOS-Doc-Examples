/****************************************************************************
 *
 * Copy right:   2024-, Copyrigths of EigenComm Ltd.
 * File name:    can_demo.c
 * Description:  EC718 CAN driver example entry source file
 * History:      Rev1.0   2024-12-04
 *
 ****************************************************************************/
#include <stdio.h>
#include "app.h"
#include "bsp_can.h"


/*
   This project demotrates the usage of CAN controller, in this example, we'll perform CAN self test first, then conduct can message tx and rx test.
 */

#define RetCheck(cond, v1)                  \
do{                                         \
    if((cond))                              \
    {                                       \
        printf("%s OK\n", (v1));            \
    } else {                                \
        printf("%s Failed !!!\n", (v1));    \
        while(1);                           \
    }                                       \
}while(FALSE)

#define CAN_BIT_RATE_KHZ    (125)   // 1000 500 250 125 100

typedef enum
{
    CAN_MESSAGE_TX_START,
    CAN_MESSAGE_TX_COMPLETE,
    CAN_MESSAGE_TX_ABORT_BY_BUS_OFF,
} CAN_message_tx_state;

#define MESSAGE_SEND_START   (0)

uint32_t    rx_obj_idx;
uint8_t     rx_data[8];

ARM_CAN_MSG_INFO rx_msg_info;

uint32_t    tx_obj_idx;
uint8_t     tx_data[8];
ARM_CAN_MSG_INFO tx_msg_info;

static volatile uint32_t txState;
static volatile uint32_t hasRx = 0;
static volatile uint32_t unitEventReportCnt = 0;
static volatile uint32_t unitEventHandledCnt = 0;
static volatile uint32_t unitEventReport;

char *unitEventString[] = {
                              "INACTIVE",
                              "ACTIVE",
                              "WARNING",
                              "PASSIVE",
                              "BUS_OFF"
                          };

char *errorCodeString[] = {
                              "NO_ERROR",
                              "BIT_ERROR",
                              "STUFF_ERROR",
                              "CRC_ERROR",
                              "FORM_ERROR",
                              "ACK_ERROR"
                          };
extern ARM_DRIVER_CAN Driver_CAN0;
static ARM_DRIVER_CAN *canDrvInstance = &Driver_CAN0;

void CAN_SignalUnitEvent(uint32_t event)
{
    unitEventReportCnt++;
    unitEventReport = event;

    if((txState == CAN_MESSAGE_TX_START) && (event == ARM_CAN_EVENT_UNIT_BUS_OFF))
    {
        txState = CAN_MESSAGE_TX_ABORT_BY_BUS_OFF;
    }
}

void CAN_SignalObjectEvent(uint32_t obj_idx, uint32_t event)
{
    if(obj_idx == rx_obj_idx)
    {
        if(event == ARM_CAN_EVENT_RECEIVE)
        {
            hasRx = 1;
        }
    }

    if(obj_idx == tx_obj_idx)
    {
       if(event == ARM_CAN_EVENT_SEND_COMPLETE)
       {
            txState = CAN_MESSAGE_TX_COMPLETE;
       }
    }
}
void CAN_messageDump(ARM_CAN_MSG_INFO * msg, uint8_t data[])
{
    printf("id: %x, extended: %d, dlc: %d, rtr: %d\ndata:", msg->id & 0x1fffffff, !!(msg->id & 0x80000000), msg->dlc, msg->rtr);

    for(uint32_t j = 0; j < msg->dlc; j++)
    {
        printf("%2x ", data[j]);
    }
    printf("\n");
}

void CAN_CheckAndHandleUniteEventReport(void)
{
    uint32_t ret, mask, unitEvent, lostReport = 0;

    mask = SaveAndSetIRQMask();

    if(unitEventReportCnt != unitEventHandledCnt)
    {
        unitEvent = unitEventReport;

        // Report is too fast to handle which means we have lost some unit event reports

        lostReport = (unitEventReportCnt != (unitEventHandledCnt + 1));

        unitEventHandledCnt = unitEventReportCnt;

        RestoreIRQMask(mask);

        printf("unit state changed to: %s, report lost: %d\n", unitEventString[unitEvent], lostReport);

        if(unitEvent == ARM_CAN_EVENT_UNIT_BUS_OFF)
        {
            ret = canDrvInstance->Control(ARM_CAN_RECOVER_FROM_BUS_OFF, 0);

            RetCheck(ret == ARM_DRIVER_OK, "Recover from bus off");

        }

    }
    RestoreIRQMask(mask);
}

//#define ENABLE_SELF_TEST

void CAN_ExampleEntry(void)
{
    printf("CAN Example Start\r\n");

    int32_t ret;
    uint32_t num_objects;

    volatile uint32_t timeout_ms = 5000;

    volatile int32_t rx_message_cnt, sendNum = 1, tx_send_ok = 1;

    ARM_CAN_STATUS can_status;

    // In case AGPIO is used
    slpManAONIOPowerOn();

    ARM_CAN_CAPABILITIES can_cap;
    ARM_CAN_OBJ_CAPABILITIES can_obj_cap;

    can_cap = canDrvInstance->GetCapabilities();
    num_objects = can_cap.num_objects;

    ret = canDrvInstance->Initialize(CAN_SignalUnitEvent, CAN_SignalObjectEvent);
    RetCheck(ret == ARM_DRIVER_OK, "Initialize");

    ret = canDrvInstance->PowerControl(ARM_POWER_FULL);
    RetCheck(ret == ARM_DRIVER_OK, "Power on");

    ret = canDrvInstance->SetMode(ARM_CAN_MODE_INITIALIZATION);
    RetCheck(ret == ARM_DRIVER_OK, "Activate initialization mode");

#if CAN_BIT_RATE_KHZ == 25
    ret = canDrvInstance->SetBitrate(ARM_CAN_BITRATE_NOMINAL,
                                     CAN_BIT_RATE_KHZ * 1000,
                                     ARM_CAN_BIT_PROP_SEG(9U) |
                                     ARM_CAN_BIT_PHASE_SEG1(6U) |
                                     ARM_CAN_BIT_PHASE_SEG2(4U) |
                                     ARM_CAN_BIT_SJW(1U)
                                     );
#else
    ret = canDrvInstance->SetBitrate(ARM_CAN_BITRATE_NOMINAL,
                                     CAN_BIT_RATE_KHZ * 1000,
                                     ARM_CAN_BIT_PROP_SEG(5U) |
                                     ARM_CAN_BIT_PHASE_SEG1(4U) |
                                     ARM_CAN_BIT_PHASE_SEG2(3U) |
                                     ARM_CAN_BIT_SJW(1U)
                                     );
#endif    

    RetCheck(ret == ARM_DRIVER_OK, "Set bitrate");

    rx_obj_idx = 0xFFFFFFFFU;
    tx_obj_idx = 0xFFFFFFFFU;

    for(uint32_t i = 0; i < num_objects; i++)
    {
        can_obj_cap = canDrvInstance->ObjectGetCapabilities(i);

        if((rx_obj_idx == 0xFFFFFFFFU) && (can_obj_cap.rx == 1U))
        {
            rx_obj_idx = i;
        }
        else if((tx_obj_idx == 0xFFFFFFFFU) && (can_obj_cap.tx == 1U))
        {
            tx_obj_idx = i;
            break;
        }
    }

    if((rx_obj_idx == 0xFFFFFFFFU) || (tx_obj_idx == 0xFFFFFFFFU))
    {
        while(1);
    }

    ret = canDrvInstance->ObjectSetFilter(rx_obj_idx, ARM_CAN_FILTER_ID_EXACT_ADD, ARM_CAN_EXTENDED_ID(0x12345678U), 0);
    RetCheck(ret == ARM_DRIVER_OK, "Set filter");

    ret = canDrvInstance->ObjectConfigure(tx_obj_idx, ARM_CAN_OBJ_TX);
    RetCheck(ret == ARM_DRIVER_OK, "Configure tx obj");

    ret = canDrvInstance->ObjectConfigure(rx_obj_idx, ARM_CAN_OBJ_RX);
    RetCheck(ret == ARM_DRIVER_OK, "Configure tx obj");

    memset(&tx_msg_info, 0, sizeof(tx_msg_info));

    tx_msg_info.id = ARM_CAN_EXTENDED_ID(0x12345678U);

    for(uint32_t i = 0; i < 8; i++)
    {
        tx_data[i] = i;
    }

#ifdef ENABLE_SELF_TEST
    ret = canDrvInstance->SetMode( ARM_CAN_MODE_LOOPBACK_EXTERNAL);
    RetCheck(ret == ARM_DRIVER_OK, "Activate external loopback mode");

    timeout_ms = 5000;
    txState = CAN_MESSAGE_TX_START;

    ret = canDrvInstance->MessageSend(tx_obj_idx, &tx_msg_info, tx_data, 8);

    RetCheck(ret == 8, "self test tx sends");

    do
    {
        delay_us(1000);
    }
    while((--timeout_ms) && (txState == CAN_MESSAGE_TX_START));

    if(timeout_ms == 0)
    {
        printf("self test tx sends timeout\n");
        while(1);
    }
    else
    {
        printf("self test tx send done\n");
    }

    timeout_ms = 5000;

    do
    {
        delay_us(1000);
    }
    while((--timeout_ms) && (hasRx == 0));

    hasRx = 0;

    if(timeout_ms == 0)
    {
        printf("self test rx timeout\n");
        while(1);
    }
    else
    {
        printf("self test rx done\n");

        memset(&rx_msg_info, 0, sizeof(rx_msg_info));

        canDrvInstance->MessageRead(rx_obj_idx, &rx_msg_info, rx_data, 8);

        CAN_messageDump(&rx_msg_info, rx_data);

    }  

#endif

    ret = canDrvInstance->SetMode( ARM_CAN_MODE_NORMAL);
    RetCheck(ret == ARM_DRIVER_OK, "Activate nomal mode");

#if 0
    ret = canDrvInstance->Control(ARM_CAN_CONTROL_RETRANSMISSION, 0);
    RetCheck(ret == ARM_DRIVER_OK, "Set single shot mode");
#endif

#if 0
    ret = canDrvInstance->PowerControl(ARM_POWER_OFF);
    RetCheck(ret, "Power on");
    while(1);
#endif

    while(1)
    {

        CAN_CheckAndHandleUniteEventReport();
#if 1
        sendNum = 1;
        for(; (sendNum <= 8) && (tx_send_ok == 1); sendNum++)
        {
            txState = CAN_MESSAGE_TX_START;
            ret = canDrvInstance->MessageSend(tx_obj_idx, &tx_msg_info, tx_data, sendNum);

            if(ret != sendNum)
            {
                printf("tx send %d fails, error code: %d\n", sendNum, ret);
                while(1);
            }

            timeout_ms = 5000;

            do
            {
                delay_us(1000);
                CAN_CheckAndHandleUniteEventReport();
                can_status = canDrvInstance->GetStatus();
            }
            while((--timeout_ms) && (txState == CAN_MESSAGE_TX_START) && (can_status.last_error_code == ARM_CAN_LEC_NO_ERROR));

            if(txState == CAN_MESSAGE_TX_ABORT_BY_BUS_OFF)
            {
                printf("tx send fails for bus off\n");

                // Wait until recover from bus off
                while(can_status.unit_state == ARM_CAN_UNIT_STATE_BUS_OFF)
                {
                    CAN_CheckAndHandleUniteEventReport();
                    can_status = canDrvInstance->GetStatus();
                }
            }
            else if(txState == CAN_MESSAGE_TX_COMPLETE)
            {
                printf("tx send %d done\n", sendNum);
            }
            else
            {
                printf("error %s occured in tx\n", errorCodeString[can_status.last_error_code]);
                canDrvInstance->Control(ARM_CAN_ABORT_MESSAGE_SEND, tx_obj_idx);
            }

        }
#endif

        if(hasRx == 1)
        {
            while((rx_message_cnt = canDrvInstance->GetRxMessageCount(rx_obj_idx)))
            {

                for(uint32_t i = 0; i < rx_message_cnt; i++)
                {
                    memset(&rx_msg_info, 0, sizeof(rx_msg_info));

                    canDrvInstance->MessageRead(rx_obj_idx, &rx_msg_info, rx_data, 8);

                    CAN_messageDump(&rx_msg_info, rx_data);
                }
            }
            hasRx = 0;
        }
    }
}
