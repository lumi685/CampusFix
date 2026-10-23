-- CampusFix W1-D5 P0 baseline: 16 business tables. Applied by the migration runner.
CREATE TABLE users (
    id INTEGER PRIMARY KEY,
    username TEXT NOT NULL UNIQUE CHECK (length(username) BETWEEN 3 AND 64),
    password_hash TEXT NOT NULL,
    display_name TEXT NOT NULL CHECK (length(display_name) BETWEEN 1 AND 80),
    role TEXT NOT NULL CHECK (role IN ('reporter','worker','dispatcher','sysadmin')),
    status TEXT NOT NULL DEFAULT 'ACTIVE' CHECK (status IN ('ACTIVE','DISABLED')),
    created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
    updated_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
    last_login_at TEXT
);
CREATE INDEX idx_users_status_role ON users(status, role);

CREATE TABLE auth_sessions (
    id TEXT PRIMARY KEY NOT NULL,
    user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    token_hash TEXT NOT NULL UNIQUE,
    expires_at TEXT NOT NULL,
    revoked_at TEXT,
    created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
    last_seen_at TEXT
);
CREATE INDEX idx_auth_sessions_user_revoked_expires ON auth_sessions(user_id, revoked_at, expires_at);

CREATE TABLE locations (
    id INTEGER PRIMARY KEY,
    parent_id INTEGER REFERENCES locations(id) ON DELETE RESTRICT,
    code TEXT NOT NULL UNIQUE,
    name TEXT NOT NULL,
    location_type TEXT NOT NULL CHECK (location_type IN ('CAMPUS','BUILDING','FLOOR','ROOM','AREA')),
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
    updated_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now'))
);
CREATE INDEX idx_locations_parent_active ON locations(parent_id, active);

CREATE TABLE management_grants (
    id INTEGER PRIMARY KEY,
    user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE RESTRICT,
    capability TEXT NOT NULL CHECK (capability IN
        ('AREA_SCOPE','BUSINESS_READ','REVIEW','DISPATCH','CANCEL','CLOSE',
         'WORKER_MANAGE','STATS_VIEW','INTERNAL_NOTE_READ')),
    scope_location_id INTEGER REFERENCES locations(id) ON DELETE RESTRICT,
    granted_by INTEGER NOT NULL REFERENCES users(id) ON DELETE RESTRICT,
    granted_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
    revoked_at TEXT
);
CREATE UNIQUE INDEX uq_management_grants_active
    ON management_grants(user_id, capability, COALESCE(scope_location_id, 0))
    WHERE revoked_at IS NULL;
CREATE INDEX idx_management_grants_user_revoked ON management_grants(user_id, revoked_at);
CREATE INDEX idx_management_grants_scope_capability ON management_grants(scope_location_id, capability);
CREATE INDEX idx_management_grants_granted_by ON management_grants(granted_by);

CREATE TABLE skills (
    id INTEGER PRIMARY KEY,
    code TEXT NOT NULL UNIQUE,
    name TEXT NOT NULL,
    requires_certificate INTEGER NOT NULL DEFAULT 0 CHECK (requires_certificate IN (0,1)),
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
    updated_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now'))
);
CREATE INDEX idx_skills_active ON skills(active);

CREATE TABLE fault_categories (
    id INTEGER PRIMARY KEY,
    parent_id INTEGER REFERENCES fault_categories(id) ON DELETE RESTRICT,
    code TEXT NOT NULL UNIQUE,
    name TEXT NOT NULL,
    required_skill_id INTEGER NOT NULL REFERENCES skills(id) ON DELETE RESTRICT,
    default_priority TEXT NOT NULL DEFAULT 'NORMAL'
        CHECK (default_priority IN ('LOW','NORMAL','HIGH','URGENT')),
    target_completion_minutes INTEGER NOT NULL CHECK (target_completion_minutes > 0),
    requires_certificate INTEGER NOT NULL DEFAULT 0 CHECK (requires_certificate IN (0,1)),
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
    updated_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now'))
);
CREATE INDEX idx_fault_categories_parent_active ON fault_categories(parent_id, active);
CREATE INDEX idx_fault_categories_required_skill ON fault_categories(required_skill_id);

CREATE TABLE workers (
    id INTEGER PRIMARY KEY,
    user_id INTEGER NOT NULL UNIQUE REFERENCES users(id) ON DELETE RESTRICT,
    worker_no TEXT NOT NULL UNIQUE,
    team_name TEXT,
    service_area_id INTEGER NOT NULL REFERENCES locations(id) ON DELETE RESTRICT,
    work_phone TEXT,
    availability TEXT NOT NULL DEFAULT 'AVAILABLE'
        CHECK (availability IN ('AVAILABLE','BUSY','OFF_DUTY','DISABLED')),
    max_concurrent_tasks INTEGER NOT NULL DEFAULT 1 CHECK (max_concurrent_tasks > 0),
    emergency_duty INTEGER NOT NULL DEFAULT 0 CHECK (emergency_duty IN (0,1)),
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
    updated_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now'))
);
CREATE INDEX idx_workers_area_availability_active ON workers(service_area_id, availability, active);

CREATE TABLE worker_skills (
    worker_id INTEGER NOT NULL REFERENCES workers(id) ON DELETE RESTRICT,
    skill_id INTEGER NOT NULL REFERENCES skills(id) ON DELETE RESTRICT,
    level INTEGER NOT NULL CHECK (level BETWEEN 1 AND 5),
    certified INTEGER NOT NULL DEFAULT 0 CHECK (certified IN (0,1)),
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    effective_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
    updated_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
    PRIMARY KEY (worker_id, skill_id)
);
CREATE INDEX idx_worker_skills_skill_active_certified ON worker_skills(skill_id, active, certified);

CREATE TABLE order_number_sequences (
    business_date TEXT PRIMARY KEY NOT NULL
        CHECK (length(business_date) = 8 AND business_date NOT GLOB '*[^0-9]*'),
    last_value INTEGER NOT NULL DEFAULT 0 CHECK (last_value BETWEEN 0 AND 9999),
    updated_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now'))
);

CREATE TABLE work_orders (
    id INTEGER PRIMARY KEY,
    order_no TEXT UNIQUE,
    reporter_id INTEGER NOT NULL REFERENCES users(id) ON DELETE RESTRICT,
    category_id INTEGER NOT NULL REFERENCES fault_categories(id) ON DELETE RESTRICT,
    location_id INTEGER NOT NULL REFERENCES locations(id) ON DELETE RESTRICT,
    title TEXT NOT NULL CHECK (length(title) BETWEEN 1 AND 120),
    description TEXT NOT NULL CHECK (length(description) BETWEEN 1 AND 2000),
    requested_urgency TEXT NOT NULL DEFAULT 'NORMAL'
        CHECK (requested_urgency IN ('LOW','NORMAL','HIGH','URGENT')),
    approved_urgency TEXT CHECK (approved_urgency IN ('LOW','NORMAL','HIGH','URGENT')),
    expected_visit_at TEXT,
    contact_name TEXT NOT NULL,
    contact_phone TEXT NOT NULL,
    status TEXT NOT NULL DEFAULT 'DRAFT'
        CHECK (status IN ('DRAFT','PENDING_REVIEW','PENDING_SUPPLEMENT',
                          'PENDING_DISPATCH','PENDING_ACCEPT','IN_PROGRESS',
                          'PENDING_VERIFY','COMPLETED','CLOSED','CANCELLED')),
    version INTEGER NOT NULL DEFAULT 1 CHECK (version > 0),
    repair_summary TEXT,
    rework_count INTEGER NOT NULL DEFAULT 0 CHECK (rework_count >= 0),
    created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
    updated_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
    submitted_at TEXT,
    completed_at TEXT,
    closed_at TEXT,
    cancelled_at TEXT
);
CREATE INDEX idx_work_orders_reporter_created ON work_orders(reporter_id, created_at DESC);
CREATE INDEX idx_work_orders_status_updated ON work_orders(status, updated_at DESC);
CREATE INDEX idx_work_orders_category_status ON work_orders(category_id, status);
CREATE INDEX idx_work_orders_location_status ON work_orders(location_id, status);

CREATE TABLE attachments (
    id INTEGER PRIMARY KEY,
    order_id INTEGER NOT NULL REFERENCES work_orders(id) ON DELETE RESTRICT,
    uploader_id INTEGER NOT NULL REFERENCES users(id) ON DELETE RESTRICT,
    purpose TEXT NOT NULL CHECK (purpose IN ('REPORT','REPAIR_RESULT')),
    original_name TEXT NOT NULL,
    stored_path TEXT NOT NULL UNIQUE,
    mime TEXT NOT NULL CHECK (mime IN ('image/jpeg','image/png','image/webp')),
    size_bytes INTEGER NOT NULL CHECK (size_bytes BETWEEN 1 AND 5242880),
    sha256 TEXT NOT NULL,
    created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
    deleted_at TEXT
);
CREATE INDEX idx_attachments_order_purpose_deleted ON attachments(order_id, purpose, deleted_at);

CREATE TABLE assignments (
    id INTEGER PRIMARY KEY,
    order_id INTEGER NOT NULL REFERENCES work_orders(id) ON DELETE RESTRICT,
    worker_id INTEGER NOT NULL REFERENCES workers(id) ON DELETE RESTRICT,
    assigned_by INTEGER NOT NULL REFERENCES users(id) ON DELETE RESTRICT,
    assign_reason TEXT,
    assigned_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
    accepted_at TEXT,
    result_submitted_at TEXT,
    ended_at TEXT,
    end_reason TEXT,
    version INTEGER NOT NULL DEFAULT 1 CHECK (version > 0)
);
CREATE UNIQUE INDEX uq_assignments_active_order ON assignments(order_id) WHERE ended_at IS NULL;
CREATE INDEX idx_assignments_worker_ended ON assignments(worker_id, ended_at);
CREATE INDEX idx_assignments_order_assigned ON assignments(order_id, assigned_at);
CREATE INDEX idx_assignments_assigned_by ON assignments(assigned_by);

CREATE TABLE transfer_requests (
    id INTEGER PRIMARY KEY,
    order_id INTEGER NOT NULL REFERENCES work_orders(id) ON DELETE RESTRICT,
    assignment_id INTEGER NOT NULL REFERENCES assignments(id) ON DELETE RESTRICT,
    requested_by INTEGER NOT NULL REFERENCES users(id) ON DELETE RESTRICT,
    reason TEXT NOT NULL CHECK (length(reason) BETWEEN 1 AND 500),
    status TEXT NOT NULL DEFAULT 'PENDING'
        CHECK (status IN ('PENDING','APPROVED','REJECTED','SUPERSEDED')),
    reviewed_by INTEGER REFERENCES users(id) ON DELETE RESTRICT,
    review_reason TEXT,
    created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
    resolved_at TEXT,
    order_version INTEGER NOT NULL CHECK (order_version > 0)
);
CREATE UNIQUE INDEX uq_transfer_requests_pending_assignment
    ON transfer_requests(assignment_id) WHERE status = 'PENDING';
CREATE INDEX idx_transfer_requests_order_status ON transfer_requests(order_id, status);
CREATE INDEX idx_transfer_requests_requested_by ON transfer_requests(requested_by);
CREATE INDEX idx_transfer_requests_reviewed_by ON transfer_requests(reviewed_by);

CREATE TABLE status_logs (
    id INTEGER PRIMARY KEY,
    order_id INTEGER NOT NULL REFERENCES work_orders(id) ON DELETE RESTRICT,
    actor_id INTEGER REFERENCES users(id) ON DELETE RESTRICT,
    assignment_id INTEGER REFERENCES assignments(id) ON DELETE RESTRICT,
    action TEXT NOT NULL,
    from_status TEXT CHECK (from_status IN
        ('DRAFT','PENDING_REVIEW','PENDING_SUPPLEMENT','PENDING_DISPATCH',
         'PENDING_ACCEPT','IN_PROGRESS','PENDING_VERIFY','COMPLETED','CLOSED','CANCELLED')),
    to_status TEXT NOT NULL CHECK (to_status IN
        ('DRAFT','PENDING_REVIEW','PENDING_SUPPLEMENT','PENDING_DISPATCH',
         'PENDING_ACCEPT','IN_PROGRESS','PENDING_VERIFY','COMPLETED','CLOSED','CANCELLED')),
    public_note TEXT,
    internal_note TEXT,
    metadata_json TEXT,
    created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
    request_id TEXT NOT NULL
);
CREATE INDEX idx_status_logs_order_created_id ON status_logs(order_id, created_at, id);
CREATE INDEX idx_status_logs_actor_created ON status_logs(actor_id, created_at);
CREATE INDEX idx_status_logs_assignment ON status_logs(assignment_id);
CREATE UNIQUE INDEX uq_status_logs_arrived_assignment
    ON status_logs(assignment_id) WHERE action = 'WORKER_ARRIVED';

CREATE TABLE dispatch_records (
    id INTEGER PRIMARY KEY,
    order_id INTEGER NOT NULL REFERENCES work_orders(id) ON DELETE RESTRICT,
    requested_by INTEGER NOT NULL REFERENCES users(id) ON DELETE RESTRICT,
    selected_worker_id INTEGER NOT NULL REFERENCES workers(id) ON DELETE RESTRICT,
    assignment_id INTEGER NOT NULL UNIQUE REFERENCES assignments(id) ON DELETE RESTRICT,
    order_version INTEGER NOT NULL CHECK (order_version > 0),
    strategy TEXT NOT NULL DEFAULT 'WEIGHTED_SCORE_V1' CHECK (strategy = 'WEIGHTED_SCORE_V1'),
    weights_json TEXT NOT NULL,
    candidates_json TEXT NOT NULL,
    excluded_summary_json TEXT NOT NULL,
    created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now'))
);
CREATE INDEX idx_dispatch_records_order_created ON dispatch_records(order_id, created_at DESC);
CREATE INDEX idx_dispatch_records_requested_by ON dispatch_records(requested_by);
CREATE INDEX idx_dispatch_records_selected_worker ON dispatch_records(selected_worker_id);

CREATE TABLE evaluations (
    id INTEGER PRIMARY KEY,
    order_id INTEGER NOT NULL UNIQUE REFERENCES work_orders(id) ON DELETE RESTRICT,
    reporter_id INTEGER NOT NULL REFERENCES users(id) ON DELETE RESTRICT,
    score INTEGER NOT NULL CHECK (score BETWEEN 1 AND 5),
    comment TEXT CHECK (comment IS NULL OR length(comment) <= 1000),
    created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now'))
);
CREATE INDEX idx_evaluations_reporter_created ON evaluations(reporter_id, created_at);
