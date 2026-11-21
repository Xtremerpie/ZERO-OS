/* ZERO KERNEL - ZERO Intelligence Subsystem Self-Test Interface (Milestone 8.0.0).
 *
 * Validates the AI Assistant daemon, provider dispatch, model manager,
 * automation permission broker, engine session context, memory persistence,
 * and security audit bridge.
 */
#ifndef _ZERO_SELFTEST_INTELLIGENCE_H
#define _ZERO_SELFTEST_INTELLIGENCE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Execute all ZERO Intelligence subsystem self-tests */
bool intelligence_run_selftests(void);

/* Generate structured intelligence diagnostic report */
void intelligence_report_generate(void);

#ifdef __cplusplus
}
#endif

#endif /* _ZERO_SELFTEST_INTELLIGENCE_H */
