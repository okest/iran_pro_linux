/***************************************************************
*  Copyright 2014 (c) Discretix Technologies Ltd.              *
*  This software is protected by copyright, international      *
*  treaties and various patents. Any copy, reproduction or     *
*  otherwise use of this software must be authorized in a      *
*  license agreement and include this Copyright Notice and any *
*  other notices specified in the license agreement.           *
*                                                              *
*  This software shall be governed by, and may be used and     *
*  redistributed under the terms and conditions of the GNU     *
*  General Public License version 2, as published by the       *
*  Free Software Foundation.                                   *
*                                                              *
*  This software is distributed in the hope that it will be    *
*  useful, but WITHOUT ANY liability and WARRANTY; without     *
*  even the implied warranty of MERCHANTABILITY or FITNESS     *
*  FOR A PARTICULAR PURPOSE. See the GNU General Public        *
*  License for more details.                                   *
*                                                              *
*  You should have received a copy of the GNU General          *
*  Public License along with this software; if not, please     *
*  write to the Free Software Foundation, Inc.,                *
*  59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.   *
****************************************************************/

 
#ifndef DX_PAL_TYPES_PLAT_H
#define DX_PAL_TYPES_PLAT_H
/* Linux kernel types */

#include <linux/types.h>

#define DX_NULL NULL

typedef unsigned int            DxUint_t;
typedef uint8_t                 DxUint8_t;
typedef uint16_t                DxUint16_t;
typedef uint32_t                DxUint32_t;
typedef uint64_t                DxUint64_t;

typedef int                   	DxInt_t;
typedef int8_t                  DxInt8_t;
typedef int16_t                 DxInt16_t;
typedef int32_t                 DxInt32_t;
typedef int64_t	                DxInt64_t;

typedef char                    DxChar_t;
typedef short                   DxWideChar_t;

#endif /*DX_PAL_TYPES_PLAT_H*/
