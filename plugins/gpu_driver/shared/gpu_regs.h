#pragma once

/**
 * gpu_regs.h - GPU hardware register offset definitions
 *
 * Register offsets must align with the actual hardware design. These definitions
 * ensure that both the emulator (UsrLinuxEmu) and real kernel driver use the
 * same register layout.
 *
 * Shared via symlink: UsrLinuxEmu/plugins/gpu_driver/shared -> TaskRunner/shared
 */

#include "gpu_types.h"

/* ========================================================================
 * GPFIFO / Channel Registers
 * ======================================================================== */

/* GPFIFO PUT pointer register (written by CPU to advance the ring) */
#define GPU_REG_GPFIFO_PUT          0x0000

/* GPFIFO GET pointer register (read by CPU to check HW progress) */
#define GPU_REG_GPFIFO_GET          0x0004

/* GPFIFO base address register (physical address of ring buffer) */
#define GPU_REG_GPFIFO_BASE_LO      0x0008
#define GPU_REG_GPFIFO_BASE_HI      0x000C

/* GPFIFO capacity register (number of entries, must be power of 2) */
#define GPU_REG_GPFIFO_CAPACITY     0x0010

/* Doorbell register (write triggers Hardware Puller state machine) */
#define GPU_REG_DOORBELL            0x0014

/* ========================================================================
 * MMU Registers
 * ======================================================================== */

/* Page table base address (physical) */
#define GPU_REG_MMU_PT_BASE_LO      0x1000
#define GPU_REG_MMU_PT_BASE_HI      0x1004

/* TLB invalidation range start */
#define GPU_REG_TLB_INVAL_START_LO  0x1008
#define GPU_REG_TLB_INVAL_START_HI  0x100C

/* TLB invalidation range end */
#define GPU_REG_TLB_INVAL_END_LO    0x1010
#define GPU_REG_TLB_INVAL_END_HI    0x1014

/* TLB invalidation trigger (write 1 to initiate) */
#define GPU_REG_TLB_INVAL_TRIGGER   0x1018

/* ========================================================================
 * PCIe / DMA Registers
 * ======================================================================== */

/* DMA source address */
#define GPU_REG_DMA_SRC_LO          0x2000
#define GPU_REG_DMA_SRC_HI          0x2004

/* DMA destination address */
#define GPU_REG_DMA_DST_LO          0x2008
#define GPU_REG_DMA_DST_HI          0x200C

/* DMA transfer size in bytes */
#define GPU_REG_DMA_SIZE            0x2010

/* DMA control register */
#define GPU_REG_DMA_CTRL            0x2014
#define   GPU_DMA_CTRL_START        (1U << 0)   /* Start DMA transfer */
#define   GPU_DMA_CTRL_DIR_H2D      (0U << 1)   /* Host to Device */
#define   GPU_DMA_CTRL_DIR_D2H      (1U << 1)   /* Device to Host */

/* DMA status register */
#define GPU_REG_DMA_STATUS          0x2018
#define   GPU_DMA_STATUS_IDLE       (1U << 0)   /* DMA engine idle */
#define   GPU_DMA_STATUS_ERROR      (1U << 1)   /* DMA error occurred */

/* ========================================================================
 * Interrupt / MSI-X Registers
 * ======================================================================== */

/* Interrupt status register (read to determine interrupt source) */
#define GPU_REG_IRQ_STATUS          0x3000

/* Interrupt clear register (write 1 to clear corresponding bit) */
#define GPU_REG_IRQ_CLEAR           0x3004

/* Interrupt mask register (write 1 to enable, 0 to mask) */
#define GPU_REG_IRQ_MASK            0x3008

/* IRQ source bits */
#define GPU_IRQ_GPFIFO_COMPLETE     (1U << 0)   /* GPFIFO processing complete */
#define GPU_IRQ_DMA_COMPLETE        (1U << 1)   /* DMA transfer complete */
#define GPU_IRQ_PAGE_FAULT          (1U << 2)   /* MMU page fault */
#define GPU_IRQ_ECC_ERROR           (1U << 3)   /* ECC memory error */

/* ========================================================================
 * CXL.cache Registers (for fused CPU/GPU device)
 * ======================================================================== */

/* CXL coherence control register */
#define GPU_REG_CXL_CTRL            0x4000
#define   GPU_CXL_CTRL_ENABLE       (1U << 0)   /* Enable CXL.cache protocol */
#define   GPU_CXL_CTRL_SNOOP_EN     (1U << 1)   /* Enable snooping */

/* CXL snoop filter status */
#define GPU_REG_CXL_SF_STATUS       0x4004
