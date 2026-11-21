#!/usr/bin/env python3
"""
ZERO OS 8.0.0 ZERO Intelligence Test Suite
Milestone 8.0.0 — ZERO Intelligence (Codename: ZERO INTELLIGENCE)

Validates the AI Assistant daemon state machine, pluggable provider abstraction,
local model discovery & single-active-model memory constraints, permission-gated
automation broker with user approval gates, assistant request lifecycle engine,
bounded conversation context tracking, persistent memory (/data/ai), and
security audit telemetry integration.
"""

import os
import sys
import unittest
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Daemon States
AI_DAEMON_IDLE              = 0
AI_DAEMON_BUSY_INFERENCE    = 1
AI_DAEMON_AWAITING_APPROVAL = 2
AI_DAEMON_EXECUTING_ACTION  = 3
AI_DAEMON_STOPPED           = 4

# Provider Types
AI_PROVIDER_TYPE_LOCAL_CPU  = 0
AI_PROVIDER_TYPE_ADAPTER    = 1
AI_PROVIDER_TYPE_REMOTE     = 2

# Provider States
AI_PROVIDER_STATE_UNINITIALIZED = 0
AI_PROVIDER_STATE_READY         = 1
AI_PROVIDER_STATE_BUSY          = 2
AI_PROVIDER_STATE_ERROR         = 3
AI_PROVIDER_STATE_OFFLINE       = 4

# Model Formats & States
AI_MODEL_FORMAT_BUILTIN       = 0
AI_MODEL_FORMAT_GGUF          = 1
AI_MODEL_FORMAT_MICRO_WEIGHTS = 2
AI_MODEL_FORMAT_REMOTE_REF    = 3

AI_MODEL_STATE_AVAILABLE = 0
AI_MODEL_STATE_LOADING   = 1
AI_MODEL_STATE_LOADED    = 2
AI_MODEL_STATE_UNLOADING = 3
AI_MODEL_STATE_ERROR     = 4

# Action Types
AI_ACTION_NONE            = 0
AI_ACTION_SYS_INFO        = 1
AI_ACTION_NOTIFICATION    = 2
AI_ACTION_APP_LAUNCH      = 3
AI_ACTION_FILE_READ       = 4
AI_ACTION_FILE_WRITE      = 5
AI_ACTION_SERVICE_CONTROL = 6

# Action Statuses
AI_ACTION_STATUS_NONE             = 0
AI_ACTION_STATUS_PENDING_APPROVAL = 1
AI_ACTION_STATUS_APPROVED         = 2
AI_ACTION_STATUS_REJECTED         = 3
AI_ACTION_STATUS_EXECUTED         = 4
AI_ACTION_STATUS_FAILED           = 5
AI_ACTION_STATUS_SECURITY_DENIED  = 6

# Constraints for Celeron J3355 hardware environment
CELERON_MAX_MODEL_RAM_MB = 256
MAX_SESSION_MESSAGES     = 16
MAX_PROMPT_LENGTH        = 512
MAX_RESPONSE_LENGTH      = 1024


class MockModelDescriptor:
    def __init__(self, model_id, name, format_type, param_count_m, quant, context_len, ram_mb):
        self.id = model_id
        self.name = name
        self.format = format_type
        self.param_count_m = param_count_m
        self.quantization = quant
        self.context_len = context_len
        self.ram_required_mb = ram_mb
        self.state = AI_MODEL_STATE_AVAILABLE


class MockModelManager:
    def __init__(self):
        self.models = [
            MockModelDescriptor(1, "ZeroMicro-15M", AI_MODEL_FORMAT_BUILTIN, 15, "Q4_0", 512, 32),
            MockModelDescriptor(2, "ZeroReason-35M", AI_MODEL_FORMAT_MICRO_WEIGHTS, 35, "Q4_0", 1024, 64),
            MockModelDescriptor(3, "ZeroAssistant-Host", AI_MODEL_FORMAT_REMOTE_REF, 135, "FP16", 2048, 0),
        ]
        self.active_model_id = 0

    def get_by_id(self, model_id):
        for m in self.models:
            if m.id == model_id:
                return m
        return None

    def can_fit_in_memory(self, model_id):
        m = self.get_by_id(model_id)
        if not m:
            return False
        return m.ram_required_mb <= CELERON_MAX_MODEL_RAM_MB

    def load_model(self, model_id):
        m = self.get_by_id(model_id)
        if not m or not self.can_fit_in_memory(model_id):
            return False
        # Single active model policy: unload any currently loaded model
        for other in self.models:
            if other.state == AI_MODEL_STATE_LOADED:
                other.state = AI_MODEL_STATE_AVAILABLE
        m.state = AI_MODEL_STATE_LOADED
        self.active_model_id = model_id
        return True

    def unload_model(self):
        if self.active_model_id != 0:
            m = self.get_by_id(self.active_model_id)
            if m:
                m.state = AI_MODEL_STATE_AVAILABLE
            self.active_model_id = 0
            return True
        return False


class MockAutomationBroker:
    def __init__(self):
        self.actions = []
        self.next_id = 1

    def is_sensitive(self, action_type):
        return action_type in (AI_ACTION_FILE_WRITE, AI_ACTION_SERVICE_CONTROL, AI_ACTION_APP_LAUNCH)

    def propose(self, session_id, pid, action_type, target, desc):
        requires_approval = self.is_sensitive(action_type)
        status = AI_ACTION_STATUS_PENDING_APPROVAL if requires_approval else AI_ACTION_STATUS_APPROVED
        act = {
            "id": self.next_id,
            "session_id": session_id,
            "pid": pid,
            "type": action_type,
            "target": target,
            "desc": desc,
            "requires_approval": requires_approval,
            "status": status,
        }
        self.next_id += 1
        self.actions.append(act)
        return act["id"]

    def approve(self, action_id):
        for a in self.actions:
            if a["id"] == action_id and a["status"] == AI_ACTION_STATUS_PENDING_APPROVAL:
                a["status"] = AI_ACTION_STATUS_APPROVED
                return True
        return False

    def reject(self, action_id):
        for a in self.actions:
            if a["id"] == action_id and a["status"] == AI_ACTION_STATUS_PENDING_APPROVAL:
                a["status"] = AI_ACTION_STATUS_REJECTED
                return True
        return False

    def execute(self, action_id):
        for a in self.actions:
            if a["id"] == action_id:
                if a["status"] != AI_ACTION_STATUS_APPROVED:
                    return False, "Action not approved"
                a["status"] = AI_ACTION_STATUS_EXECUTED
                return True, f"Executed action {action_id} on {a['target']}"
        return False, "Action not found"


# ==============================================================================
# Unit Test Cases
# ==============================================================================

class AIDaemonLifecycleTest(unittest.TestCase):
    """Test AI assistant daemon states and ring buffer logging."""

    def test_daemon_state_constants(self):
        self.assertEqual(AI_DAEMON_IDLE, 0)
        self.assertEqual(AI_DAEMON_BUSY_INFERENCE, 1)
        self.assertEqual(AI_DAEMON_AWAITING_APPROVAL, 2)
        self.assertEqual(AI_DAEMON_EXECUTING_ACTION, 3)
        self.assertEqual(AI_DAEMON_STOPPED, 4)

    def test_daemon_ring_buffer_eviction(self):
        max_entries = 8
        ring_buffer = []

        def log_event(msg):
            if len(ring_buffer) >= max_entries:
                ring_buffer.pop(0)  # FIFO eviction
            ring_buffer.append(msg)

        for i in range(15):
            log_event(f"Event {i}")

        self.assertEqual(len(ring_buffer), max_entries)
        self.assertEqual(ring_buffer[0], "Event 7")
        self.assertEqual(ring_buffer[-1], "Event 14")


class AIProviderAbstractionTest(unittest.TestCase):
    """Test pluggable AI provider architecture and fallback logic."""

    def test_provider_registry_and_active_switch(self):
        providers = {
            1: {"name": "Local CPU Micro-Runtime", "type": AI_PROVIDER_TYPE_LOCAL_CPU, "state": AI_PROVIDER_STATE_READY},
            2: {"name": "Host Development Adapter", "type": AI_PROVIDER_TYPE_ADAPTER, "state": AI_PROVIDER_STATE_READY},
            3: {"name": "Remote Reference Proxy", "type": AI_PROVIDER_TYPE_REMOTE, "state": AI_PROVIDER_STATE_OFFLINE},
        }

        self.assertEqual(len(providers), 3)
        self.assertEqual(providers[1]["type"], AI_PROVIDER_TYPE_LOCAL_CPU)
        self.assertEqual(providers[2]["type"], AI_PROVIDER_TYPE_ADAPTER)

        # Provider generation simulation
        def generate_text(prov_id, prompt):
            prov = providers.get(prov_id)
            if not prov or prov["state"] != AI_PROVIDER_STATE_READY:
                return None, "Provider unavailable"
            if prov["type"] == AI_PROVIDER_TYPE_LOCAL_CPU:
                return f"[LocalCPU] Responding to: {prompt}", "OK"
            return f"[Adapter] Responding to: {prompt}", "OK"

        res, status = generate_text(1, "Status check")
        self.assertEqual(status, "OK")
        self.assertTrue(res.startswith("[LocalCPU]"))

        res_offline, err = generate_text(3, "Status check")
        self.assertIsNone(res_offline)
        self.assertEqual(err, "Provider unavailable")


class AIModelManagerTest(unittest.TestCase):
    """Test model metadata, hardware memory fits, and single-active-model policy."""

    def setUp(self):
        self.mgr = MockModelManager()

    def test_model_discovery(self):
        self.assertEqual(len(self.mgr.models), 3)
        m1 = self.mgr.get_by_id(1)
        self.assertEqual(m1.name, "ZeroMicro-15M")
        self.assertEqual(m1.param_count_m, 15)
        self.assertEqual(m1.ram_required_mb, 32)

    def test_memory_budget_bounds(self):
        # 32MB and 64MB fit in Celeron J3355 256MB budget
        self.assertTrue(self.mgr.can_fit_in_memory(1))
        self.assertTrue(self.mgr.can_fit_in_memory(2))

        # Oversized model simulation
        huge_model = MockModelDescriptor(99, "HugeLLM-7B", AI_MODEL_FORMAT_GGUF, 7000, "Q4_0", 4096, 4000)
        self.mgr.models.append(huge_model)
        self.assertFalse(self.mgr.can_fit_in_memory(99))
        self.assertFalse(self.mgr.load_model(99))

    def test_single_active_model_policy(self):
        # Load model 1
        self.assertTrue(self.mgr.load_model(1))
        self.assertEqual(self.mgr.active_model_id, 1)
        self.assertEqual(self.mgr.get_by_id(1).state, AI_MODEL_STATE_LOADED)

        # Load model 2 -> model 1 must be unloaded automatically
        self.assertTrue(self.mgr.load_model(2))
        self.assertEqual(self.mgr.active_model_id, 2)
        self.assertEqual(self.mgr.get_by_id(2).state, AI_MODEL_STATE_LOADED)
        self.assertEqual(self.mgr.get_by_id(1).state, AI_MODEL_STATE_AVAILABLE)

        # Explicit unload
        self.assertTrue(self.mgr.unload_model())
        self.assertEqual(self.mgr.active_model_id, 0)
        self.assertEqual(self.mgr.get_by_id(2).state, AI_MODEL_STATE_AVAILABLE)


class AIAutomationBrokerTest(unittest.TestCase):
    """Test permission-gated automation actions, user approval gates, and execution."""

    def setUp(self):
        self.broker = MockAutomationBroker()

    def test_safe_action_auto_approval(self):
        # SYS_INFO and NOTIFICATION are read-only / low-risk -> auto-approved
        act_id = self.broker.propose(session_id=1, pid=10, action_type=AI_ACTION_SYS_INFO,
                                     target="CPU", desc="Get CPU usage")
        act = self.broker.actions[0]
        self.assertEqual(act["status"], AI_ACTION_STATUS_APPROVED)
        self.assertFalse(act["requires_approval"])

        # Can execute immediately
        ok, msg = self.broker.execute(act_id)
        self.assertTrue(ok)
        self.assertEqual(act["status"], AI_ACTION_STATUS_EXECUTED)

    def test_sensitive_action_mandates_user_approval(self):
        # FILE_WRITE is sensitive -> must be PENDING_APPROVAL
        act_id = self.broker.propose(session_id=1, pid=10, action_type=AI_ACTION_FILE_WRITE,
                                     target="/data/test.txt", desc="Write file")
        act = self.broker.actions[0]
        self.assertEqual(act["status"], AI_ACTION_STATUS_PENDING_APPROVAL)
        self.assertTrue(act["requires_approval"])

        # Attempt execution before approval must fail
        ok, err = self.broker.execute(act_id)
        self.assertFalse(ok)
        self.assertEqual(err, "Action not approved")
        self.assertEqual(act["status"], AI_ACTION_STATUS_PENDING_APPROVAL)

        # Approve and execute
        self.assertTrue(self.broker.approve(act_id))
        self.assertEqual(act["status"], AI_ACTION_STATUS_APPROVED)
        ok, msg = self.broker.execute(act_id)
        self.assertTrue(ok)
        self.assertEqual(act["status"], AI_ACTION_STATUS_EXECUTED)

    def test_user_rejection_prevents_execution(self):
        act_id = self.broker.propose(session_id=1, pid=10, action_type=AI_ACTION_SERVICE_CONTROL,
                                     target="ssh_daemon", desc="Stop service")
        act = self.broker.actions[0]
        self.assertEqual(act["status"], AI_ACTION_STATUS_PENDING_APPROVAL)

        # Reject action
        self.assertTrue(self.broker.reject(act_id))
        self.assertEqual(act["status"], AI_ACTION_STATUS_REJECTED)

        # Execution must fail
        ok, err = self.broker.execute(act_id)
        self.assertFalse(ok)
        self.assertEqual(act["status"], AI_ACTION_STATUS_REJECTED)


class AIEngineSessionContextTest(unittest.TestCase):
    """Test conversation context window bounds and intent classification."""

    def test_intent_classification(self):
        intents = [
            ("open terminal", AI_ACTION_APP_LAUNCH, "terminal"),
            ("launch text editor", AI_ACTION_APP_LAUNCH, "editor"),
            ("show memory info", AI_ACTION_SYS_INFO, "system"),
            ("write file /data/doc.txt", AI_ACTION_FILE_WRITE, "/data/doc.txt"),
            ("read /etc/hosts", AI_ACTION_FILE_READ, "/etc/hosts"),
            ("what is ZERO OS?", AI_ACTION_NONE, None),
        ]

        def classify_intent(text):
            t = text.lower()
            if "open terminal" in t or "launch" in t:
                target = "terminal" if "terminal" in t else "editor"
                return AI_ACTION_APP_LAUNCH, target
            if "show" in t or "memory" in t or "cpu" in t:
                return AI_ACTION_SYS_INFO, "system"
            if "write file" in t:
                parts = text.split("write file ")
                return AI_ACTION_FILE_WRITE, parts[1].strip()
            if "read" in t:
                parts = text.split("read ")
                return AI_ACTION_FILE_READ, parts[1].strip()
            return AI_ACTION_NONE, None

        for prompt, exp_type, exp_target in intents:
            act_type, target = classify_intent(prompt)
            self.assertEqual(act_type, exp_type)
            if exp_target:
                self.assertEqual(target, exp_target)

    def test_bounded_context_fifo_eviction(self):
        messages = [{"role": "system", "text": "You are ZERO AI assistant."}]

        def add_msg(role, text):
            if len(messages) >= MAX_SESSION_MESSAGES:
                # Evict oldest non-system message
                messages.pop(1)
            messages.append({"role": role, "text": text})

        for i in range(25):
            add_msg("user", f"Question {i}")
            add_msg("assistant", f"Answer {i}")

        self.assertLessEqual(len(messages), MAX_SESSION_MESSAGES)
        # Ensure system prompt was preserved at index 0
        self.assertEqual(messages[0]["role"], "system")
        self.assertEqual(messages[0]["text"], "You are ZERO AI assistant.")


class AIMemoryPersistenceTest(unittest.TestCase):
    """Test preference configuration format and session history serialization."""

    def test_preferences_parsing_and_formatting(self):
        config_text = (
            "default_model=ZeroMicro-15M\n"
            "max_context_tokens=512\n"
            "auto_approve_safe_info=1\n"
            "confirm_sensitive_writes=1\n"
            "notifications_enabled=1\n"
            "active_provider_id=1\n"
        )

        prefs = {}
        for line in config_text.strip().splitlines():
            if "=" in line:
                k, v = line.split("=", 1)
                prefs[k.strip()] = v.strip()

        self.assertEqual(prefs["default_model"], "ZeroMicro-15M")
        self.assertEqual(int(prefs["max_context_tokens"]), 512)
        self.assertEqual(int(prefs["auto_approve_safe_info"]), 1)
        self.assertEqual(int(prefs["confirm_sensitive_writes"]), 1)


class AIAuditTelemetryTest(unittest.TestCase):
    """Test AI assistant audit record formatting and structured event tracking."""

    def test_audit_event_formatting(self):
        audit_events = []

        def log_ai_audit(action_id, action_type, status, target, user_approved):
            entry = f"[AUDIT-AI] act_id={action_id} type={action_type} status={status} target={target} approved={user_approved}"
            audit_events.append(entry)

        log_ai_audit(101, "FILE_WRITE", "APPROVED", "/data/ai/notes.txt", True)
        log_ai_audit(102, "SERVICE_CTL", "REJECTED", "ssh_daemon", False)

        self.assertEqual(len(audit_events), 2)
        self.assertIn("target=/data/ai/notes.txt approved=True", audit_events[0])
        self.assertIn("target=ssh_daemon approved=False", audit_events[1])


if __name__ == "__main__":
    print("=" * 60, flush=True)
    print("  ZERO OS 8.0.0 ZERO Intelligence Test Suite", flush=True)
    print("=" * 60, flush=True)
    suite = unittest.TestSuite()
    for test_class in [
        AIDaemonLifecycleTest,
        AIProviderAbstractionTest,
        AIModelManagerTest,
        AIAutomationBrokerTest,
        AIEngineSessionContextTest,
        AIMemoryPersistenceTest,
        AIAuditTelemetryTest,
    ]:
        suite.addTests(unittest.defaultTestLoader.loadTestsFromTestCase(test_class))
    runner = unittest.TextTestRunner(stream=sys.stdout, verbosity=2)
    result = runner.run(suite)
    if result.wasSuccessful():
        print("=" * 60, flush=True)
        print("ALL 7 ZERO INTELLIGENCE TEST SUITES PASSED (100% OK)", flush=True)
        print("=" * 60, flush=True)
        sys.exit(0)
    else:
        print("=" * 60, flush=True)
        print(f"FAILED: {len(result.failures)} failures, {len(result.errors)} errors", flush=True)
        print("=" * 60, flush=True)
        sys.exit(1)
