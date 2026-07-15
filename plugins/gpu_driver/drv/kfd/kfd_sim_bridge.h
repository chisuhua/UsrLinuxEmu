/*
 * kfd_sim_bridge.h — KFD ↔ sim bridge declarations
 *
 * C-12 B.3.5: Provides kfd_sim_bridge_set_hal() for Phase B.3.4 mock impl.
 * Actual registration is performed by the hal mock module.
 *
 * Architecture: ②-③ bridge (per ADR-036 three-way separation).
 */

#pragma once
#ifdef __cplusplus
extern "C" {
#endif

/* Forward declare — actual definition provided by hal/gpu_hal.h */
struct gpu_hal_ops;

/* kfd_sim_bridge_set_hal — Register HAL ops pointer for sim ↔ drv bridging.
 *
 * @hal: pointer to initialized gpu_hal_ops (lifetime managed by caller)
 *
 * Phase B.3.4: mock impl stores the pointer (used by hal_mock.cpp).
 * Phase C: real implementation dispatches KFD events via HAL.
 */
void kfd_sim_bridge_set_hal(struct gpu_hal_ops *hal);

/* kfd_sim_bridge_get_hal — Read back registered HAL pointer (test-only). */
struct gpu_hal_ops *kfd_sim_bridge_get_hal(void);

#ifdef __cplusplus
}
#endif