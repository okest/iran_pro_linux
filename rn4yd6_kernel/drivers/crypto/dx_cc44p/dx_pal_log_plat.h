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

/* Dummy pal_log_plat for test driver in kernel */

#ifndef _DX_PAL_LOG_PLAT_H_
#define _DX_PAL_LOG_PLAT_H_

#if defined(DEBUG)

#define __DX_PAL_LOG_PLAT(level, format, ...) printk(level "cc44_test::" format , ##__VA_ARGS__)

#else /* Disable all prints */

#define __DX_PAL_LOG_PLAT(...)  do {} while (0)

#endif

#endif /*_DX_PAL_LOG_PLAT_H_*/

