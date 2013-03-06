/************************************************************
 * Copyright (C), 2008-2012,
 * FileName:		// 文件名
 * Author:			// 作者
 * Date:			// 日期
 * Description:		// 模块描述
 * Version:			// 版本信息
 * Function List:	// 主要函数及其功能
 *     1. -------
 * History:			// 历史修改记录
 *     <author>  <time>   <version >   <desc>
 *     David    96/10/12     1.0     build this moudle
 ***********************************************************/
#include <stdint.h>
#include "board.h"
#include "stm32f4xx.h"
#include <rtthread.h>
#include "scr.h"



#define LCD_Y_PAGE	4
#define LCD_X_COL	122

#define LCD_MODE_CLEAR     0
#define LCD_MODE_SET       1
#define LCD_MODE_XOR       2
#define LCD_MODE_INVERT		3


#define SCRN_LEFT		0
#define SCRN_TOP		0
#define SCRN_RIGHT		121
#define SCRN_BOTTOM		31




const unsigned char l_mask_array[8] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };   /* TODO: avoid or PROGMEM */

/* the LCD display image memory */
/* buffer arranged so page memory is sequential in RAM */
static unsigned char l_display_array[LCD_Y_PAGE][LCD_X_COL];

/* control-lines hardware-interface (only "write") */


/*

   #define MR		(1<<15)
   #define SHCP	(1<<12)
   #define DS		(1<<14)
   #define STCP1	(1<<15)
   #define STCP2	(1<<13)
 */

#define RST0	( 1 << 0 )
#define CS		( 1 << 1 )
#define EN		( 1 << 2 )
#define RW		( 1 << 3 )
#define A0		( 1 << 4 )
#define BL		( 1 << 6 )      /*Backlight*/

#define STCP_DATA	GPIO_Pin_15 /*数据接口D0..7*/
#define DS			GPIO_Pin_14
#define STCP_CTRL	GPIO_Pin_13 /*控制接口*/
#define SHCP		GPIO_Pin_12


/***********************************************************
* Function:
* Description:
* Input:
* Input:
* Output:
* Return:
* Others:
***********************************************************/
static void ControlBitShift( unsigned char data )
{
	unsigned char i;

	//IOSET0	= STCP1;
	//IOCLR0 = STCP2;
	GPIO_SetBits( GPIOE, STCP_DATA );
	GPIO_ResetBits( GPIOE, STCP_CTRL );

	for( i = 0; i < 8; i++ )
	{
		GPIO_ResetBits( GPIOE, SHCP );      //IOCLR0=SHCP;
		if( data & 0x80 )
		{
			GPIO_SetBits( GPIOE, DS );      //IOSET0 = DS;
		}else
		{
			GPIO_ResetBits( GPIOE, DS );    //IOCLR0 = DS;
		}
		GPIO_SetBits( GPIOE, SHCP );        //IOSET0 = SHCP;
		data <<= 1;
	}

	GPIO_SetBits( GPIOE, STCP_CTRL );       //IOSET0 = STCP2;
}

/***********************************************************
* Function:
* Description:
* Input:
* Input:
* Output:
* Return:
* Others:
***********************************************************/
static void DataBitShift( unsigned char data )
{
	unsigned char i;
	GPIO_SetBits( GPIOE, STCP_CTRL );
	GPIO_ResetBits( GPIOE, STCP_DATA );
	for( i = 0; i < 8; i++ )
	{
		GPIO_ResetBits( GPIOE, SHCP );      //IOCLR0=SHCP;
		if( data & 0x80 )
		{
			GPIO_SetBits( GPIOE, DS );      //IOSET0 = DS;
		}else
		{
			GPIO_ResetBits( GPIOE, DS );    //IOCLR0 = DS;
		}
		GPIO_SetBits( GPIOE, SHCP );        //IOSET0 = SHCP;
		data <<= 1;
	}
	GPIO_SetBits( GPIOE, STCP_DATA );       //IOSET0 = STCP1;
}


static void st7565_reset(void)
{
	ControlBitShift( RST0 | BL );
	rt_thread_delay(1);
	ControlBitShift(BL);
	rt_thread_delay(1);
	ControlBitShift( RST0 | BL );
}


/*
**
** low level routine to send a byte value
** to the LCD controller control register.
** entry argument is the data to output
** and the controller to use
** 1: IC 1, 2: IC 2, 3: both ICs
**
*/
static void st7565_ctl( const unsigned char cmd )
{
	unsigned int	i;
	ControlBitShift( RST0|CS|EN | BL );
	DataBitShift( cmd );
	for(i=0;i<0xf;i++){}
	ControlBitShift( RST0| EN|BL );
}

/*
**
** low level routine to send a byte value
** to the LCD controller data register. entry argument
** is the data to output and the controller-number
**
*/
static void st7565_dat( const unsigned char dat )
{
	unsigned int i;

	ControlBitShift( RST0 | CS|EN|A0 | BL );
	DataBitShift( dat );
	for(i=0;i<0xf;i++){}
	ControlBitShift( RST0 | A0 |EN| BL );
}



void lcd_init( void )
{
	
	st7565_reset();
	st7565_ctl( 0xe3 );  /*NOP*/
	st7565_ctl( 0xa2 );  /*BAIS= 1/9*/
	st7565_ctl( 0xa1);  /* ADC 0 S0->S121*/
	st7565_ctl( 0xc0);  /*SET SHK C31->C0*/
	st7565_ctl( 0xf8 );  /*VC ON*/
	st7565_ctl( 0x00 );
	st7565_ctl( 0x2c );
	st7565_ctl( 0x2e );
	st7565_ctl( 0x2f );
	st7565_ctl( 0x81 );
	st7565_ctl( 0x0f );
	st7565_ctl( 0x27 );  /*分压电阻*/
	st7565_ctl( 0xaf );  /*Display ON*/
	st7565_ctl( 0x40 );	/*First　Line COM0 */
}


/*
**
** Updates area of the display. Writes data from "framebuffer"
** RAM to the lcd display controller RAM.
**
** Arguments Used:
**    top     top line of area to update.
**    bottom  bottom line of area to update.
**    from MJK-Code
**
*/
void lcd_update( const unsigned char top, const unsigned char bottom )
{
	unsigned char	x;
	unsigned char	y;
	unsigned char	yt;
	unsigned char	yb;
	unsigned char	*colptr;

	/* setup bytes of range */
	yb	= bottom >> 3;
	yt	= top >> 3;

	for( y = yt; y <= yb; y++ )
	{
		st7565_ctl( 0xb0 + y ); /* set page */
		st7565_ctl( 0x10);
		st7565_ctl( 0x00+10);
		colptr = &l_display_array[y][0];

		for( x = 0; x < LCD_X_COL; x++ )
		{
			st7565_dat( *colptr++);
		}
	}
}

/***********************************************************
* Function:
* Description:
* Input:
* Input:
* Output:
* Return:
* Others:
***********************************************************/
void lcd_update_all( void )
{
	lcd_update( SCRN_TOP, SCRN_BOTTOM );
}



/* fill buffer and LCD with pattern */
void lcd_fill( const unsigned char pattern )
{
	unsigned char page, col;
	lcd_init( );
	st7565_ctl( 0xae );
	for( page = 0; page < LCD_Y_PAGE; page++ )
	{
		for( col = 0; col < LCD_X_COL; col++ )
		{
			l_display_array[page][col] = pattern;
		}
	}
	lcd_update_all( );
	st7565_ctl( 0xaf );
}



/*
图片生成模式 PC2LCD2000中

逐行、顺向、阴码

*/
void lcd_bitmap( const uint8_t left, const uint8_t top, const struct IMG_DEF *img_ptr, const uint8_t mode )
{
	uint8_t width, heigth, h, w, pattern, mask;
	uint8_t * ptable;

	uint8_t bitnum, bitmask;
	uint8_t page, col, vdata;

	width	= img_ptr->width_in_pixels;
	heigth	= img_ptr->height_in_pixels;
	ptable	= (uint8_t*)( img_ptr->char_table );

	mask	= 0x80;
	pattern = *ptable;

	for( h = 0; h < heigth; h++ ) /**/
	{
		page	= ( h + top ) >> 3;
		bitnum	= ( h + top ) & 0x07;
		bitmask = ( 1 << bitnum );
		for( w = 0; w < width; w++ )
		{
			col		= left + w;
			vdata	= l_display_array[page][col];
			switch( mode )
			{
				case LCD_MODE_SET: /*不管原来的数据，直接设置为pattern的值*/
					if( pattern & mask )
					{
						vdata |= bitmask;
					} else
					{
						vdata &= ~bitmask;
					}
					break;
				case LCD_MODE_CLEAR:    /*不管原来的数据，清除原来的值=>0*/
					vdata &= ~bitmask;
					break;
				case LCD_MODE_XOR:      /*原来的数据，直接设置为pattern的值*/
					if( vdata & bitmask )
					{
						if( pattern & mask )
						{
							vdata &= ~bitmask;
						} else
						{
							vdata |= bitmask;
						}
					}else
					{
						if( pattern & mask )
						{
							vdata |= bitmask;
						} else
						{
							vdata &= ~bitmask;
						}
					}
					break;
				case LCD_MODE_INVERT: /*不管原来的数据，直接设置为pattern的值*/
					if( pattern & mask )
					{
						vdata &= ~bitmask;
					} else
					{
						vdata |= bitmask;
					}
					break;
			}
			l_display_array[page][col]	= vdata;
			mask						>>= 1;
			if( mask == 0 )
			{
				mask = 0x80;
				ptable++;
				pattern = *ptable;
			}
		}
		if( mask != 0x80 ) /*一行中的列已处理完*/
		{
			mask = 0x80;
			ptable++;
			pattern = *ptable;
		}
	}
}

/*
   绘制12点阵的字符，包括中文和英文


 */
void lcd_text12( char left, char top, char *p, char len, const char mode )
{
	int				charnum = len;
	int				i;
	char			msb, lsb;

	int				addr;
	unsigned char	start_col = left;
	unsigned int	val_old, val_new, val_mask;

	unsigned int	glyph[12]; /*保存一个字符的点阵信息，以逐列式*/

	while( charnum )
	{
		for( i = 0; i < 12; i++ )
		{
			glyph[i] = 0;
		}
		msb = *p++;
		charnum--;
		if( msb <= 0x80 ) //ascii字符 0612
		{
			addr = ( msb - 0x20 ) * 12 + FONT_ASC0612_ADDR;
			for( i = 0; i < 3; i++ )
			{
				val_new				= *(__IO uint32_t*)addr;
				glyph[i * 2 + 0]	= ( val_new & 0xffff );
				glyph[i * 2 + 1]	= ( val_new & 0xffff0000 ) >> 16;
				addr				+= 4;
			}

			val_mask = ( ( 0xfff ) << top ); /*12bit*/

			/*加上top的偏移*/
			for( i = 0; i < 6; i++ )
			{
				glyph[i] <<= top;

				val_old = l_display_array[0][start_col] | ( l_display_array[1][start_col] << 8 ) | ( l_display_array[2][start_col] << 16 ) | ( l_display_array[3][start_col] << 24 );
				if( mode == LCD_MODE_SET )
				{
					val_new = val_old & ( ~val_mask ) | glyph[i];
				}else if( mode == LCD_MODE_INVERT )
				{
					val_new = ( val_old | val_mask ) & ( ~glyph[i] );
				}
				l_display_array[0][start_col]	= val_new & 0xff;
				l_display_array[1][start_col]	= ( val_new & 0xff00 ) >> 8;
				l_display_array[2][start_col]	= ( val_new & 0xff0000 ) >> 16;
				l_display_array[3][start_col]	= ( val_new & 0xff000000 ) >> 24;
				start_col++;
			}
		}else
		{
			lsb = *p++;
			charnum--;
			if( ( msb >= 0xa1 ) && ( msb <= 0xa3 ) && ( lsb >= 0xa1 ) )
			{
				addr = FONT_HZ1212_ADDR + ( ( ( (unsigned long)msb ) - 0xa1 ) * 94 + ( ( (unsigned long)lsb ) - 0xa1 ) ) * 24;
			}else if( ( msb >= 0xb0 ) && ( msb <= 0xf7 ) && ( lsb >= 0xa1 ) )
			{
				addr = FONT_HZ1212_ADDR + ( ( ( (unsigned long)msb ) - 0xb0 ) * 94 + ( ( (unsigned long)lsb ) - 0xa1 ) ) * 24 + 282 * 24;
			}
			for( i = 0; i < 6; i++ )
			{
				val_new				= *(__IO uint32_t*)addr;
				glyph[i * 2 + 0]	= ( val_new & 0xffff );
				glyph[i * 2 + 1]	= ( val_new & 0xffff0000 ) >> 16;
				addr				+= 4;
			}
			val_mask = ( ( 0xfff ) << top ); /*12bit*/

			/*加上top的偏移*/
			for( i = 0; i < 12; i++ )
			{
				glyph[i] <<= top;
				/*通过start_col映射到I_display_array中，注意mask*/
				val_old = l_display_array[0][start_col] | ( l_display_array[1][start_col] << 8 ) | ( l_display_array[2][start_col] << 16 ) | ( l_display_array[3][start_col] << 24 );
				if( mode == LCD_MODE_SET )
				{
					val_new = val_old & ( ~val_mask ) | glyph[i];
				}else if( mode == LCD_MODE_INVERT )
				{
					val_new = ( val_old | val_mask ) & ( ~glyph[i] );
				}
				l_display_array[0][start_col]	= val_new & 0xff;
				l_display_array[1][start_col]	= ( val_new & 0xff00 ) >> 8;
				l_display_array[2][start_col]	= ( val_new & 0xff0000 ) >> 16;
				l_display_array[3][start_col]	= ( val_new & 0xff000000 ) >> 24;
				start_col++;
			}
		}
	}
}

/************************************** The End Of File **************************************/
