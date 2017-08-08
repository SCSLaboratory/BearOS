/*
 Copyright <2017> <Scaleable and Concurrent Systems Lab; 
                   Thayer School of Engineering at Dartmouth College>

 Permission is hereby granted, free of charge, to any person obtaining a copy 
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights 
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
 copies of the Software, and to permit persons to whom the Software is 
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
*/

#include <sbin/static_ip.h>

struct node_desc static_ip_table[STATIC_IP_TABLE_SIZE] = {

	/* Node 0 */
	{
		{0x00,0x23,0xAE,0xFE,0x28,0xF8}, /* MAC: 00:23:AE:FE:28:F8 */ 
		{1799465601},                    /* IP:  129.170.65.107    */
		{16318463},                      /* NM:  255.255.248.0     */
		{21015169}                       /* GW:  129.170.64.1      */
	},

	/* Node 1 */
	{
		{0x00,0x23,0xAE,0xFE,0x28,0x00}, /* MAC: 00:23:AE:FB:E0:99 */ 
		{1849797249},                    /* IP:  129.170.65.107    */
		{16318463},                      /* NM:  255.255.248.0     */
		{21015169}                       /* GW:  129.170.64.1      */
	},

		/* Node 0 steve */
	{
		{0x00,0x23,0xAE,0xFE,0x2B,0xB8}, 
		{1866574465},                    /* IP:  129.170.65.111    */
		{16318463},                      /* NM:  255.255.248.0     */
		{21015169}                       /* GW:  129.170.64.1      */
	}, 

	{
		{0xb0, 0xc4,0x20,0x00,0x00,0x01}, 
		{50374848},                    /* IP:  192.168.0.8    */
		{16777215},                      /* NM:  255.255.255.0     */
		{33597632}                       /* GW:  192.168.0.2      */
	}

};
