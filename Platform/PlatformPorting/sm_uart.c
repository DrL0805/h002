/**********************************************************************
**
**模块说明: 对接MCU UART驱动接口，并根据项目原理图作配置
**软件版本，修改日志(时间，内容),修改人:
**   V1.0   2018.4.14  初版  ZSL  
**
**********************************************************************/
//在此模块中使用UART IO口，因此需定义UART_MODULE
#define UART_MODULE
#include "io_config.h"

#if(((UART0_TX != IO_UNKNOW) && (UART0_RX != IO_UNKNOW)) || ((UART1_TX != IO_UNKNOW) && (UART1_RX != IO_UNKNOW)))
#include "am_mcu_apollo.h"
#include "sm_gpio.h"
#include "string.h"
#include "sm_sys.h"
#include "sm_uart.h"

//定义UART口
enum
{
    UART_ID0=0,
    UART_ID1,
    UART_MAX,
};

//记录上层注册的callback
static uart_cb pUart_CallBack[UART_MAX];

//软件定义的缓冲buffer大小，此值可根据实际情况调整
#define UART1_TXBUFFER_MAX 256
#define UART1_RXBUFFER_MAX 256

//uart buffer
uint8_t u8TxBuffer[UART1_TXBUFFER_MAX];
uint8_t u8RxBuffer[UART1_RXBUFFER_MAX];

//uart硬件FIFO大小是32，不可修改
#define UART_FIFO_SIZE 32
uint8_t u8fifo[UART_FIFO_SIZE];

#ifdef AM_PART_APOLLO3
void *pUart_Handler[2] = {NULL};

//UART0中断服务程序
void am_uart_isr(void)
{
    uint32_t ui32Status;

    // Service the FIFOs as necessary, and clear the interrupts.
    am_hal_uart_interrupt_status_get(pUart_Handler[UART_ID0], &ui32Status, true);
    am_hal_uart_interrupt_clear(pUart_Handler[UART_ID0], ui32Status);
    am_hal_uart_interrupt_service(pUart_Handler[UART_ID0], ui32Status, NULL);

    //UART0接收数据事件
    if (AM_HAL_UART_INT_RX & ui32Status)
    {
//        RTT_DEBUG_printf(0,"Uart rx...\n");		
        pUart_CallBack[UART_ID0](UART_EVENT_RX);
    }
    //UART0发送数据事件
    if (AM_HAL_UART_INT_TX & ui32Status)
    {
		//RTT_DEBUG_printf(0,"Uart tx...\n");
        pUart_CallBack[UART_ID0](UART_EVENT_TX);
    }
    //UART0接收数据超时事件
    if (AM_HAL_UART_INT_RX_TMOUT & ui32Status)
    {
//        RTT_DEBUG_printf(0,"Uart rx timeout...\n");		
        pUart_CallBack[UART_ID0](UART_EVENT_RX_TIMEOUT);
    }
}

//UART1中断服务程序
void am_uart1_isr(void)
{
    uint32_t ui32Status;

    // Service the FIFOs as necessary, and clear the interrupts.
    am_hal_uart_interrupt_status_get(pUart_Handler[UART_ID1], &ui32Status, true);
    am_hal_uart_interrupt_clear(pUart_Handler[UART_ID1], ui32Status);
    am_hal_uart_interrupt_service(pUart_Handler[UART_ID1], ui32Status, NULL);

    //UART0接收数据事件
    if (AM_HAL_UART_INT_RX & ui32Status)
    {
        pUart_CallBack[UART_ID1](UART_EVENT_RX);
    }
    //UART0发送数据事件
    if (AM_HAL_UART_INT_TX & ui32Status)
    {
        pUart_CallBack[UART_ID1](UART_EVENT_TX);
    }
    //UART0接收数据超时事件
    if (AM_HAL_UART_INT_RX_TMOUT & ui32Status)
    {
        pUart_CallBack[UART_ID1](UART_EVENT_RX_TIMEOUT);
    }
}

//**********************************************************************
// 函数功能: 初始化UART
// 输入参数：	
// 返回参数：
//**********************************************************************
void SMDrv_UART_Init(void)
{
    memset(pUart_CallBack,NULL,sizeof(pUart_CallBack));
}

//**********************************************************************
// 函数功能: 根据driver module ID打开硬件对应的UART
// 输入参数：	
//    modul: driver module ID,值参考uart_module
//    ptype_info:要设置的uart中断类型，FIFO类型, 若不需中断，则设置为NULL
//    ut_callback:上层注册的中断回调函数
// 返回参数：Ret_InvalidParam或Ret_OK
//**********************************************************************
ret_type SMDrv_UART_Open(uart_module modul,uart_cb ut_callback)
{
    uint32 u32RxPin = IO_UNKNOW,u32TxPin = IO_UNKNOW;
    uint32 u32TxConfig = 0x00,u32RxConfig = 0x00;
    am_hal_gpio_pincfg_t g_UartConfig = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    interrupt_handle irq_handle;
    uint8 u8uart_id;

    //默认的UART参数
    am_hal_uart_config_t UartConfig = 
    {
        // Standard UART settings: 115200-8-N-1
        .ui32BaudRate = 115200,
        .ui32DataBits = AM_HAL_UART_DATA_BITS_8,
        .ui32Parity = AM_HAL_UART_PARITY_NONE,
        .ui32StopBits = AM_HAL_UART_ONE_STOP_BIT,
        .ui32FlowControl = AM_HAL_UART_FLOW_CTRL_NONE,
    };    

    //step 1:配置模块UART参数
    if(modul == GPS_UART_MODULE)
    {
        // Set TX and RX FIFOs to interrupt at half-full.
        UartConfig.ui32FifoLevels = (AM_HAL_UART_TX_FIFO_1_2 | AM_HAL_UART_RX_FIFO_1_2);
        
        // Buffers
        UartConfig.pui8TxBuffer = u8TxBuffer;
        UartConfig.ui32TxBufferSize = sizeof(u8TxBuffer);
        UartConfig.pui8RxBuffer = u8RxBuffer;
        UartConfig.ui32RxBufferSize = sizeof(u8RxBuffer);

        //以下的code需要根据项目原理图配置
        u8uart_id = UART_ID1;
        u32RxPin = UART1_RX;
        u32TxPin = UART1_TX;
        u32TxConfig=AM_HAL_PIN_14_UART1TX;
        u32RxConfig=AM_HAL_PIN_15_UART1RX;
    }
    else if(modul == BASE_UART_MODULE)
    {
        // Set TX and RX FIFOs to interrupt at half-full.
        UartConfig.ui32FifoLevels = (AM_HAL_UART_TX_FIFO_1_2 | AM_HAL_UART_RX_FIFO_1_2);

        // Buffers 通讯数据量小，不需用大buffer
        UartConfig.pui8TxBuffer = NULL;
        UartConfig.ui32TxBufferSize = 0;
        UartConfig.pui8RxBuffer = u8fifo;
        UartConfig.ui32RxBufferSize = sizeof(u8fifo);
    
        //以下的code需要根据项目原理图配置
        u8uart_id = UART_ID0;
        u32RxPin = UART0_RX;
        u32TxPin = UART0_TX;
        u32TxConfig=AM_HAL_PIN_22_UART0TX;
        u32RxConfig=AM_HAL_PIN_23_UART0RX;
    }
    else
    {
        return Ret_InvalidParam;
    }

    //step 2:将对应的IO口配置为UART功能，并设置UART参数
    g_UartConfig.uFuncSel = u32RxConfig;
    am_hal_gpio_pinconfig(u32RxPin, g_UartConfig);
    g_UartConfig.uFuncSel = u32TxConfig;
    g_UartConfig.eDriveStrength  = AM_HAL_GPIO_PIN_DRIVESTRENGTH_2MA;
    am_hal_gpio_pinconfig(u32TxPin, g_UartConfig);

    //step 3:初始化，开启和配置uart
    if(am_hal_uart_initialize(u8uart_id, &pUart_Handler[u8uart_id]) != AM_HAL_STATUS_SUCCESS)
        Err_Info((0,"Error - init uart failed.\n"));

    if(am_hal_uart_power_control(pUart_Handler[u8uart_id], AM_HAL_SYSCTRL_WAKE, false) != AM_HAL_STATUS_SUCCESS)
        Err_Info((0,"Error - power on uart failed.\n"));

    if(am_hal_uart_configure(pUart_Handler[u8uart_id], &UartConfig) != AM_HAL_STATUS_SUCCESS)
        Err_Info((0,"Error - config uart failed.\n"));

    // Enable interrupts.
    irq_handle = (u8uart_id == UART_ID1) ? INTERRUPT_UART1 : INTERRUPT_UART0;
    SMDrv_SYS_EnableIsr(irq_handle);

    //step 4:根据中断参数使能uart中断
    if(ut_callback != NULL)
    {
        pUart_CallBack[u8uart_id] = ut_callback;
    }

    return Ret_OK;
}

//**********************************************************************
// 函数功能: 关闭driver module ID硬件对应的UART,以实现低功耗
// 输入参数：	
//    modul: driver module ID,值参考uart_module
// 返回参数：Ret_InvalidParam或Ret_OK
//**********************************************************************
ret_type SMDrv_UART_Close(uart_module modul)
{
    uint32 u32Modul = UART_MAX,u32RxPin = IO_UNKNOW,u32TxPin = IO_UNKNOW;
    am_hal_gpio_pincfg_t g_AM_HAL_GPIO_DISABLE = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    interrupt_handle irq_handle;
    uint8 u8uart_id;

    //step 1:配置模块UART参数
    if(modul == GPS_UART_MODULE)
    {
        //以下的code需要根据项目原理图配置
        u8uart_id = UART_ID1;
        u32RxPin = UART1_RX;
        u32TxPin = UART1_TX;
    }
    else if(modul == BASE_UART_MODULE)
    {
        //以下的code需要根据项目原理图配置
        u8uart_id = UART_ID0;
        u32RxPin = UART0_RX;
        u32TxPin = UART0_TX;
    }
    else
        return Ret_InvalidParam;

    //step 2:disable UART时钟，和UART
    am_hal_uart_disable(pUart_Handler[u8uart_id]);
    am_hal_uart_power_control(pUart_Handler[u8uart_id], AM_HAL_SYSCTRL_DEEPSLEEP, false);
    am_hal_uart_deinitialize(pUart_Handler[u8uart_id]);

    //step 3:重新配置uart脚功能
    g_AM_HAL_GPIO_DISABLE.uFuncSel       = 3;
    g_AM_HAL_GPIO_DISABLE.eDriveStrength = AM_HAL_GPIO_PIN_DRIVESTRENGTH_2MA;
    g_AM_HAL_GPIO_DISABLE.eGPOutcfg      = AM_HAL_GPIO_PIN_OUTCFG_DISABLE;
    am_hal_gpio_pinconfig(u32RxPin, g_AM_HAL_GPIO_DISABLE);
    am_hal_gpio_pinconfig(u32TxPin, g_AM_HAL_GPIO_DISABLE);

    //step 4:清除中断回调函数，open的时候重新设置
    irq_handle = (u8uart_id == UART_ID1) ? INTERRUPT_UART1 : INTERRUPT_UART0;
    SMDrv_SYS_DisableIsr(irq_handle);

    pUart_CallBack[u32Modul] = NULL;
    return Ret_OK;
}

//**********************************************************************
// 函数功能:  设置GPIO中断优先级,并启动uart中断
// 输入参数:
//     modul: driver module ID,值参考uart_module
//     prio:中断优先级，bit0~2位值有效
// 返回参数： 无
//**********************************************************************
ret_type SMDrv_UART_SetIsrPrio(uart_module modul,uint32 prio)
{
    interrupt_handle irq_handle;

    if(modul == GPS_UART_MODULE)
        irq_handle = INTERRUPT_UART1;
    else if(modul == BASE_UART_MODULE)
        irq_handle = INTERRUPT_UART0;
    else
        return Ret_InvalidParam;

    //step:设置中断优先级并使能uart中断
    SMDrv_SYS_SetIsrPrio(irq_handle,prio);
    SMDrv_SYS_EnableIsr(irq_handle);

    return Ret_OK;
}

//**********************************************************************
// 函数功能: 使能/失能UART某个类型的中断
// 输入参数:
//     modul: driver module ID,值参考uart_module
//     irq_type:中断类型
//     benable: 使能/失能UART某类型中断  =1--使能， =0--失能
// 返回参数： 无
//**********************************************************************
ret_type SMDrv_UART_EnableIrq(uart_module modul,uint32 irq_type,uint8 benable)
{
    uint8 u8uart_id;
    uint32_t ui32IntMask = 0x00;

    if(UART_EVENT_NONE == irq_type)
        return Ret_InvalidParam;
    
    if(modul == GPS_UART_MODULE)
        u8uart_id = UART_ID1;
    else if(modul == BASE_UART_MODULE)
        u8uart_id = UART_ID0;
    else
        return Ret_InvalidParam;

    //发送中断
    ui32IntMask |= (irq_type & UART_EVENT_TX) ? AM_HAL_UART_INT_TX : 0x00;
    //接收中断
    ui32IntMask |= (irq_type & UART_EVENT_RX) ? AM_HAL_UART_INT_RX : 0x00;
    //接收超时中断
    ui32IntMask |= (irq_type & UART_EVENT_RX_TIMEOUT)? AM_HAL_UART_INT_RX_TMOUT : 0x00;

    if(irq_type != UART_EVENT_NONE)
    {
        am_hal_uart_interrupt_clear(pUart_Handler[u8uart_id], ui32IntMask);
        if(benable == 1)
            am_hal_uart_interrupt_enable(pUart_Handler[u8uart_id], ui32IntMask);
        else
            am_hal_uart_interrupt_disable(pUart_Handler[u8uart_id], ui32IntMask);
    }
    return Ret_OK;
}

//**********************************************************************
// 函数功能: 向driver module ID对应的UART写度n个字节
// 输入参数：	
//    modul: driver module ID
//    pData: 要发生的数据
//    len: 要发生的数据的长度
// 返回参数：
//    pu16LenWritten: 实际发送的长度
//**********************************************************************
ret_type SMDrv_UART_WriteBytes(uart_module modul,uint8 *pData,uint16 len,uint16 *pu16LenWritten)
{
    am_hal_uart_transfer_t sUartWrite;
    uint32 ret;
    uint8 u8uart_id;
    
    if(modul == GPS_UART_MODULE)
        u8uart_id = UART_ID1;
    else if(modul == BASE_UART_MODULE)
        u8uart_id = UART_ID0;
    else
        return Ret_InvalidParam;

    sUartWrite.ui32Direction = AM_HAL_UART_WRITE;
    sUartWrite.pui8Data = (uint8_t *) pData;
    sUartWrite.ui32NumBytes = len;
    sUartWrite.ui32TimeoutMs = 0;
    sUartWrite.pui32BytesTransferred = (uint32_t *)pu16LenWritten;

    ret = am_hal_uart_transfer(pUart_Handler[u8uart_id], &sUartWrite);
    if(ret != AM_HAL_STATUS_SUCCESS)
    {
        //Err_Info((0,"Error - write uart failed:%d\n",ret));
    }

    return Ret_OK;
}

//**********************************************************************
// 函数功能: 向driver module ID对应的UART写n个字节
// 输入参数：	
//    modul: driver module ID
//    pBuffer: 要读取的数据缓存buffer
//    len: 要读取的数据的长度
// 返回参数：
//    pu16LenWritten: 实际读取的数据长度
//**********************************************************************
ret_type SMDrv_UART_ReadBytes(uart_module modul,uint8 *pBuffer,uint16 len,uint16 *pu16ReadLen)
{
    am_hal_uart_transfer_t sUartRead;
    uint8 u8uart_id;
    uint32 ret;
    
    if(modul == GPS_UART_MODULE)
        u8uart_id = UART_ID1;
    else if(modul == BASE_UART_MODULE)
        u8uart_id = UART_ID0;
    else
        return Ret_InvalidParam;

    sUartRead.ui32Direction = AM_HAL_UART_READ;
    sUartRead.pui8Data = (uint8_t *) pBuffer;
    sUartRead.ui32NumBytes = len;
    sUartRead.ui32TimeoutMs = 0;
    sUartRead.pui32BytesTransferred = (uint32_t *)pu16ReadLen;

    ret = am_hal_uart_transfer(pUart_Handler[u8uart_id], &sUartRead);
    if(ret != AM_HAL_STATUS_SUCCESS)
    {
        //Err_Info((0,"Error - read uart failed:%d\n",ret));
    }

    return Ret_OK;
}

//**********************************************************************
// 函数功能: 获取module使用UART发送缓冲区大小
// 输入参数：	
//    modul: driver module ID
// 返回参数：发送缓冲区大小
//**********************************************************************
uint32 SMDrv_UART_GetTxBuffSize(uart_module modul)
{
    uint32 u32BuffSize;
    if(modul == GPS_UART_MODULE)
        u32BuffSize = UART1_TXBUFFER_MAX;
    else if(modul == BASE_UART_MODULE)
        u32BuffSize = UART_FIFO_SIZE;
    else
        u32BuffSize = 0;

    return u32BuffSize;
}

//**********************************************************************
// 函数功能: 获取module使用UART接收缓冲区大小
// 输入参数：	
//    modul: driver module ID
// 返回参数：接收缓冲区大小
//**********************************************************************
uint32 SMDrv_UART_GetRxBuffSize(uart_module modul)
{
    uint32 u32BuffSize;
    if(modul == GPS_UART_MODULE)
        u32BuffSize = UART1_RXBUFFER_MAX;
    else if(modul == BASE_UART_MODULE)
        u32BuffSize = UART_FIFO_SIZE;
    else
        u32BuffSize = 0;

    return u32BuffSize;
}

#else

/*
//UART0中断服务程序
void am_uart_isr1(void)
{
    uint32 statuvalue;

    statuvalue = am_hal_uart_int_status_get(UART0,true);
    am_hal_uart_int_clear(UART0,0xFFFFFFFF);

    //UART0接收数据事件
    if (AM_HAL_UART_INT_RX & statuvalue)
    {
        pUart_CallBack[UART0](UART_EVENT_RX);
    }
    //UART0发送数据事件
    if (AM_HAL_UART_INT_TX & statuvalue)
    {
        pUart_CallBack[UART0](UART_EVENT_TX);
    }
    //UART0接收数据超时事件
    if (AM_HAL_UART_INT_RX_TMOUT & statuvalue)
    {
        pUart_CallBack[UART0](UART_EVENT_RX_TIMEOUT);
    }
}

//UART1中断服务程序
void am_uart1_isr1(void)
{
    uint32    statuvalue;
    statuvalue = am_hal_uart_int_status_get(UART1,true);
    am_hal_uart_int_clear(UART1,0xFFFFFFFF);

    //UART1接收数据事件
    if (AM_HAL_UART_INT_RX & statuvalue)
    {
        pUart_CallBack[UART1](UART_EVENT_RX);
    }
    //UART1发送数据事件
    if (AM_HAL_UART_INT_TX & statuvalue)
    {
        pUart_CallBack[UART1](UART_EVENT_TX);
    }
    //UART1接收数据超时事件
    if (AM_HAL_UART_INT_RX_TMOUT & statuvalue)
    {
        pUart_CallBack[UART1](UART_EVENT_RX_TIMEOUT);
    }
}
*/
//**********************************************************************
// 函数功能: 设置FIFO 和UART 中断类型
// 输入参数：u32Modul: driver module ID,值参考uart_module
//    ptype_info:FIFO类型和中断类型   
// 返回参数：TRUE:正常
//           FALSE:设置的中断类型无效，不可注册回调函数
//**********************************************************************
static uint8 Uart_Irq_Init(uint32 u32Modul,uart_openinfo *ptype_info)
{
    //配置UART fifo
    if(ptype_info->u32fifo_type != UART_FIFO_NONE)
    {
        am_hal_uart_fifo_config(u32Modul, ptype_info->u32fifo_type);
    }

    //设置uart中断类型及使能
    if(ptype_info->u32event_type != UART_EVENT_NONE)
    {
        am_hal_uart_int_clear(u32Modul,ptype_info->u32event_type);
        am_hal_uart_int_enable(u32Modul,ptype_info->u32event_type);
    }
    else
        return FALSE;
    return TRUE;
}

//**********************************************************************
// 函数功能: 初始化UART
// 输入参数：	
// 返回参数：
//**********************************************************************
void SMDrv_UART_Init(void)
{
    memset(pUart_CallBack,NULL,sizeof(pUart_CallBack));
}

//**********************************************************************
// 函数功能: 根据driver module ID打开硬件对应的UART
// 输入参数：	
//    modul: driver module ID,值参考uart_module
//    ptype_info:要设置的uart中断类型，FIFO类型, 若不需中断，则设置为NULL
//    ut_callback:上层注册的中断回调函数
// 返回参数：Ret_InvalidParam或Ret_OK
//
// 例子: 使能uart接收发生中断，接收fifo中数据为1/8时触发接收中断
//      uart_openinfo type_info;
//      type_info.u32event_type = UART_EVENT_RX | UART_EVENT_TX;
//      type_info.u32fifo_type =  UART_RX_FIFO_1_8;
//      SMDrv_UART_Open(GPS_UART_MODULE,&type_info,ut_callback);
//**********************************************************************
ret_type SMDrv_UART_Open(uart_module modul,uart_openinfo *ptype_info,uart_cb ut_callback)
{
    uint32 u32Modul = UART_MAX,u32RxPin = IO_UNKNOW,u32TxPin = IO_UNKNOW;
    uint32 u32TxConfig = 0x00,u32RxConfig = 0x00;
    //默认的UART参数
    am_hal_uart_config_t UartConfig = 
    {
        .ui32BaudRate   = 9600,
        .ui32DataBits   = AM_HAL_UART_DATA_BITS_8,
        .bTwoStopBits   = false,
        .ui32Parity     = AM_HAL_UART_PARITY_NONE,
        .ui32FlowCtrl   = AM_HAL_UART_FLOW_CTRL_NONE,
    };

    //step 1:配置模块UART参数
    if(modul == GPS_UART_MODULE)
    {
        u32Modul = UART1;
        u32RxPin = UART1_RX;
        u32TxPin = UART1_TX;
        u32TxConfig=AM_HAL_PIN_14_UART1TX;
        u32RxConfig=AM_HAL_PIN_15_UART1RX;
    }
    else
    {
        return Ret_InvalidParam;
    }

    //step 2:使能UART功耗管理和时钟
    am_hal_uart_pwrctrl_enable(u32Modul);	
    am_hal_uart_clock_enable(u32Modul);
    //应在关闭uart的情况下，配置UART参数
    am_hal_uart_disable(u32Modul);

    //step 3:将对应的IO口配置为UART功能，并设置UART参数
    SMDrv_GPIO_SetIOPad(u32TxPin,u32TxConfig);
    SMDrv_GPIO_SetIOPad(u32RxPin,u32RxConfig);
    am_hal_uart_config(u32Modul,&UartConfig);

    //step 4:根据中断参数使能uart中断
    if((ptype_info != NULL) && (ut_callback != NULL))
    {
        //初始化uart中断类型和fifo深度
        if(Uart_Irq_Init(u32Modul,ptype_info) == TRUE)
        {
            //只有在
            pUart_CallBack[u32Modul] = ut_callback;
        }
    }

    //step 5: 使能UART
	am_hal_uart_enable(u32Modul);
    return Ret_OK;
}

//**********************************************************************
// 函数功能: 关闭driver module ID硬件对应的UART,以实现低功耗
// 输入参数：	
//    modul: driver module ID,值参考uart_module
// 返回参数：Ret_InvalidParam或Ret_OK
//**********************************************************************
ret_type SMDrv_UART_Close(uart_module modul)
{
    uint32 u32Modul = UART_MAX,u32RxPin = IO_UNKNOW,u32TxPin = IO_UNKNOW;
    //ui32Interrupt的值为: AM_HAL_INTERRUPT_UART1,AM_HAL_INTERRUPT_UART0
    uint32 ui32Interrupt;

    //step 1:配置模块UART参数
    if(modul == GPS_UART_MODULE)
    {
        u32Modul = UART1;
        u32RxPin = UART1_RX;
        u32TxPin = UART1_TX;
        ui32Interrupt = AM_HAL_INTERRUPT_UART1;
    }
    
    if(u32Modul >= UART_MAX)
        return Ret_InvalidParam;

    //step 2:disable UART时钟，和UART
    am_hal_uart_clock_disable(u32Modul);
    am_hal_uart_disable(u32Modul);
    //step 3:重新配置uart脚功能
    SMDrv_GPIO_SetIOPad(u32RxPin,AM_HAL_PIN_INPUT |AM_HAL_GPIO_PULL24K);
    SMDrv_GPIO_SetIOPad(u32TxPin,AM_HAL_PIN_INPUT |AM_HAL_GPIO_PULL24K ); 
    //step 4:disable uart功耗管理
    am_hal_uart_pwrctrl_disable(u32Modul);

    //step 5:清除中断回调函数，open的时候重新设置
    am_hal_interrupt_disable(ui32Interrupt);
    pUart_CallBack[u32Modul] = NULL;
    return Ret_OK;
}

//**********************************************************************
// 函数功能:  设置GPIO中断优先级,并启动uart中断
// 输入参数:
//     modul: driver module ID,值参考uart_module
//     prio:中断优先级，bit0~2位值有效
// 返回参数： 无
//**********************************************************************
ret_type SMDrv_UART_SetIsrPrio(uart_module modul,uint32 prio)
{
    //ui32Interrupt的值为: AM_HAL_INTERRUPT_UART1,AM_HAL_INTERRUPT_UART0
    uint32 ui32Interrupt;
    if(modul == GPS_UART_MODULE)
        ui32Interrupt = AM_HAL_INTERRUPT_UART1;
    else
        return Ret_InvalidParam;

    //step:设置并使能uart中断
    am_hal_interrupt_priority_set(ui32Interrupt, AM_HAL_INTERRUPT_PRIORITY(prio));
    am_hal_interrupt_enable(ui32Interrupt);
    return Ret_OK;
}

//**********************************************************************
// 函数功能: 使能/失能UART某个类型的中断
// 输入参数:
//     modul: driver module ID,值参考uart_module
//     irq_type:中断类型
//     benable: 使能/失能UART某类型中断  =1--使能， =0--失能
// 返回参数： 无
//**********************************************************************
ret_type SMDrv_UART_EnableIrq(uart_module modul,uint32 irq_type,uint8 benable)
{
    uint32 u32Modul = UART_MAX;

    if(modul == GPS_UART_MODULE)
        u32Modul = UART1;
    else
        return Ret_InvalidParam;

    if(irq_type != UART_EVENT_NONE)
    {
        am_hal_uart_int_clear(u32Modul,irq_type);
        if(benable == 1)
            am_hal_uart_int_enable(u32Modul,irq_type);
        else
            am_hal_uart_int_disable(u32Modul,irq_type);
    }
    return Ret_OK;
}

//**********************************************************************
// 函数功能: 向driver module ID对应的UART写1 个字节
// 输入参数：	
//    modul: driver module ID
//    u8ch: 要发生的字符
// 返回参数：
//**********************************************************************
ret_type SMDrv_UART_WriteByte(uart_module modul,uint8 u8ch)
{
    uint32 u32Modul = UART_MAX;

    if(modul == GPS_UART_MODULE)
        u32Modul = UART1;
    else
        return Ret_InvalidParam;

    am_hal_uart_char_transmit_polled(u32Modul,u8ch);
    return Ret_OK;
}

//**********************************************************************
// 函数功能: 向driver module ID对应的UART写度多个字节
// 输入参数：	
//    modul: driver module ID
//    pData: 要发生的数据
//    len: 要发生的数据的长度
// 返回参数：
//**********************************************************************
ret_type SMDrv_UART_WriteBytes(uart_module modul,uint8 *pData,uint16 len)
{
    uint32 u32Modul = UART_MAX;

    if(modul == GPS_UART_MODULE)
        u32Modul = UART1;
    else
        return Ret_InvalidParam;

    while(len > 0)
    {
        am_hal_uart_char_transmit_polled(u32Modul,*pData);
        pData++,len--;
    }
    return Ret_OK;
}

//**********************************************************************
// 函数功能: 向driver module ID对应的UART写1 个字节
// 输入参数：	
//    modul: driver module ID,值参考uart_module
// 返回参数：读取到的值
//**********************************************************************
uint8 SMDrv_UART_ReadByte(uart_module modul)
{
    uint32 u32Modul = UART1;
    uint8 u8Char;

    if(modul == GPS_UART_MODULE)
    {
        u32Modul = UART1;
    }
    
    am_hal_uart_char_receive_polled (u32Modul,(char*)&u8Char);
    return u8Char;
}
#endif

#endif

