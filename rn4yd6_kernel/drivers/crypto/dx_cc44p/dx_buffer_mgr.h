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

/* \file buffer_mgr.h
   Buffer Manager
 */

#ifndef __BUFFER_MGR_H__
#define __BUFFER_MGR_H__

#include <crypto/algapi.h>

#include "dx_config.h"
#include "dx_driver.h"


enum dx_secure_dir_type {
	DX_NO_DMA_IS_SECURE = 0,
	DX_SRC_DMA_IS_SECURE = 1,
	DX_DST_DMA_IS_SECURE = 2
};

enum dx_req_dma_buf_type {
	DX_DMA_BUF_NULL = 0,
	DX_DMA_BUF_DLLI,
	DX_DMA_BUF_MLLI
};

enum dx_sg_cpy_direct {
	DX_SG_TO_BUF = 0,
	DX_SG_FROM_BUF = 1
};

struct dx_mlli {
	dx_sram_addr_t sram_addr;
	unsigned int nents;
};

struct mlli_params {
	struct dma_pool *curr_pool;
	uint8_t *mlli_virt_addr;
	dma_addr_t mlli_dma_addr;
	uint32_t mlli_len;  
};

int dx_buffer_mgr_init(struct dx_drvdata *drvdata);

int dx_buffer_mgr_fini(struct dx_drvdata *drvdata);

int dx_buffer_mgr_map_ablkcipher_request(struct dx_drvdata *drvdata, struct ablkcipher_request *req);

void dx_buffer_mgr_unmap_ablkcipher_request(struct device *dev, struct ablkcipher_request *req);

int dx_buffer_mgr_map_aead_request(struct dx_drvdata *drvdata, struct aead_request *req);

void dx_buffer_mgr_unmap_aead_request(struct device *dev, struct aead_request *req);

int dx_buffer_mgr_map_ahash_request_final(struct dx_drvdata *drvdata, struct ahash_request *req, bool do_update);

int dx_buffer_mgr_map_ahash_request_update(struct dx_drvdata *drvdata, struct ahash_request *req);

void dx_buffer_mgr_unmap_ahash_request(struct device *dev, struct ahash_request *req, bool do_revert);

void dx_buffer_mgr_copy_scatterlist_portion(u8 *dest, struct scatterlist *sg, int to_skip, unsigned int end, enum dx_sg_cpy_direct direct);

#endif /*__BUFFER_MGR_H__*/

