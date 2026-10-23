-- Fictional, deliberately non-loginable data. Run only with --seed after migration.
-- W3-D3 must replace the sentinel with a mature password hash before enabling accounts.
INSERT INTO users(username,password_hash,display_name,role,status) VALUES
 ('demo_sysadmin','!UNSET_DEMO_CREDENTIAL!','Demo System Admin','sysadmin','DISABLED'),
 ('demo_dispatcher','!UNSET_DEMO_CREDENTIAL!','Demo Dispatcher','dispatcher','DISABLED'),
 ('demo_reporter01','!UNSET_DEMO_CREDENTIAL!','Demo Reporter 1','reporter','DISABLED'),
 ('demo_reporter02','!UNSET_DEMO_CREDENTIAL!','Demo Reporter 2','reporter','DISABLED'),
 ('demo_reporter03','!UNSET_DEMO_CREDENTIAL!','Demo Reporter 3','reporter','DISABLED'),
 ('demo_worker01','!UNSET_DEMO_CREDENTIAL!','Demo Worker 1','worker','DISABLED'),
 ('demo_worker02','!UNSET_DEMO_CREDENTIAL!','Demo Worker 2','worker','DISABLED'),
 ('demo_worker03','!UNSET_DEMO_CREDENTIAL!','Demo Worker 3','worker','DISABLED'),
 ('demo_worker04','!UNSET_DEMO_CREDENTIAL!','Demo Worker 4','worker','DISABLED'),
 ('demo_worker05','!UNSET_DEMO_CREDENTIAL!','Demo Worker 5','worker','DISABLED'),
 ('demo_worker06','!UNSET_DEMO_CREDENTIAL!','Demo Worker 6','worker','DISABLED')
 ON CONFLICT(username) DO NOTHING;

INSERT INTO locations(code,name,location_type) VALUES
 ('DEMO-CAMPUS-A','Demo Campus A','CAMPUS'),
 ('DEMO-CAMPUS-B','Demo Campus B','CAMPUS')
 ON CONFLICT(code) DO NOTHING;
INSERT INTO locations(parent_id,code,name,location_type) VALUES
 ((SELECT id FROM locations WHERE code='DEMO-CAMPUS-A'),'DEMO-A1','Demo Building A1','BUILDING'),
 ((SELECT id FROM locations WHERE code='DEMO-CAMPUS-A'),'DEMO-A2','Demo Building A2','BUILDING'),
 ((SELECT id FROM locations WHERE code='DEMO-CAMPUS-B'),'DEMO-B1','Demo Building B1','BUILDING'),
 ((SELECT id FROM locations WHERE code='DEMO-CAMPUS-B'),'DEMO-B2','Demo Building B2','BUILDING')
 ON CONFLICT(code) DO NOTHING;
INSERT INTO locations(parent_id,code,name,location_type) VALUES
 ((SELECT id FROM locations WHERE code='DEMO-A1'),'DEMO-A1-F1','Demo A1 Floor 1','FLOOR'),
 ((SELECT id FROM locations WHERE code='DEMO-A2'),'DEMO-A2-F1','Demo A2 Floor 1','FLOOR'),
 ((SELECT id FROM locations WHERE code='DEMO-B1'),'DEMO-B1-F1','Demo B1 Floor 1','FLOOR'),
 ((SELECT id FROM locations WHERE code='DEMO-B2'),'DEMO-B2-F1','Demo B2 Floor 1','FLOOR')
 ON CONFLICT(code) DO NOTHING;
INSERT INTO locations(parent_id,code,name,location_type) VALUES
 ((SELECT id FROM locations WHERE code='DEMO-A1-F1'),'DEMO-A1-101','Demo A1 Room 101','ROOM'),
 ((SELECT id FROM locations WHERE code='DEMO-A2-F1'),'DEMO-A2-101','Demo A2 Room 101','ROOM'),
 ((SELECT id FROM locations WHERE code='DEMO-B1-F1'),'DEMO-B1-101','Demo B1 Room 101','ROOM'),
 ((SELECT id FROM locations WHERE code='DEMO-B2-F1'),'DEMO-B2-101','Demo B2 Room 101','ROOM')
 ON CONFLICT(code) DO NOTHING;

INSERT INTO skills(code,name,requires_certificate) VALUES
 ('DEMO-ELECTRICAL','Demo Electrical',1),
 ('DEMO-PLUMBING','Demo Plumbing',0),
 ('DEMO-NETWORK','Demo Network',0)
 ON CONFLICT(code) DO NOTHING;
INSERT INTO fault_categories(code,name,required_skill_id,target_completion_minutes,requires_certificate) VALUES
 ('DEMO-SOCKET','Demo Socket Fault',(SELECT id FROM skills WHERE code='DEMO-ELECTRICAL'),120,1),
 ('DEMO-PIPE','Demo Pipe Fault',(SELECT id FROM skills WHERE code='DEMO-PLUMBING'),180,0),
 ('DEMO-WIFI','Demo Network Fault',(SELECT id FROM skills WHERE code='DEMO-NETWORK'),90,0)
 ON CONFLICT(code) DO NOTHING;

INSERT INTO workers(user_id,worker_no,team_name,service_area_id,availability,max_concurrent_tasks,emergency_duty) VALUES
 ((SELECT id FROM users WHERE username='demo_worker01'),'DEMO-W001','Demo Electrical',
  (SELECT id FROM locations WHERE code='DEMO-CAMPUS-A'),'AVAILABLE',3,1),
 ((SELECT id FROM users WHERE username='demo_worker02'),'DEMO-W002','Demo Mixed',
  (SELECT id FROM locations WHERE code='DEMO-CAMPUS-A'),'AVAILABLE',2,0),
 ((SELECT id FROM users WHERE username='demo_worker03'),'DEMO-W003','Demo Plumbing',
  (SELECT id FROM locations WHERE code='DEMO-CAMPUS-A'),'OFF_DUTY',1,0),
 ((SELECT id FROM users WHERE username='demo_worker04'),'DEMO-W004','Demo Network',
  (SELECT id FROM locations WHERE code='DEMO-CAMPUS-B'),'AVAILABLE',2,0),
 ((SELECT id FROM users WHERE username='demo_worker05'),'DEMO-W005','Demo Plumbing',
  (SELECT id FROM locations WHERE code='DEMO-CAMPUS-B'),'BUSY',1,1),
 ((SELECT id FROM users WHERE username='demo_worker06'),'DEMO-W006','Demo Electrical',
  (SELECT id FROM locations WHERE code='DEMO-CAMPUS-B'),'DISABLED',3,0)
 ON CONFLICT(worker_no) DO NOTHING;

INSERT INTO worker_skills(worker_id,skill_id,level,certified) VALUES
 ((SELECT id FROM workers WHERE worker_no='DEMO-W001'),(SELECT id FROM skills WHERE code='DEMO-ELECTRICAL'),5,1),
 ((SELECT id FROM workers WHERE worker_no='DEMO-W002'),(SELECT id FROM skills WHERE code='DEMO-ELECTRICAL'),3,0),
 ((SELECT id FROM workers WHERE worker_no='DEMO-W002'),(SELECT id FROM skills WHERE code='DEMO-PLUMBING'),2,0),
 ((SELECT id FROM workers WHERE worker_no='DEMO-W003'),(SELECT id FROM skills WHERE code='DEMO-PLUMBING'),5,0),
 ((SELECT id FROM workers WHERE worker_no='DEMO-W004'),(SELECT id FROM skills WHERE code='DEMO-NETWORK'),4,0),
 ((SELECT id FROM workers WHERE worker_no='DEMO-W005'),(SELECT id FROM skills WHERE code='DEMO-PLUMBING'),4,0),
 ((SELECT id FROM workers WHERE worker_no='DEMO-W006'),(SELECT id FROM skills WHERE code='DEMO-ELECTRICAL'),2,0)
 ON CONFLICT(worker_id,skill_id) DO NOTHING;

-- Dispatcher is confined to Campus A. Sysadmin receives explicit read-only
-- business capabilities for Campus B; role alone grants no business action.
INSERT INTO management_grants(user_id,capability,scope_location_id,granted_by)
 SELECT (SELECT id FROM users WHERE username='demo_dispatcher'),'AREA_SCOPE',
        (SELECT id FROM locations WHERE code='DEMO-CAMPUS-A'),
        (SELECT id FROM users WHERE username='demo_sysadmin')
 WHERE NOT EXISTS (
   SELECT 1 FROM management_grants WHERE user_id=(SELECT id FROM users WHERE username='demo_dispatcher')
     AND capability='AREA_SCOPE' AND scope_location_id=(SELECT id FROM locations WHERE code='DEMO-CAMPUS-A')
     AND revoked_at IS NULL);
INSERT INTO management_grants(user_id,capability,scope_location_id,granted_by)
 SELECT (SELECT id FROM users WHERE username='demo_sysadmin'),seed_capabilities.value,
        (SELECT id FROM locations WHERE code='DEMO-CAMPUS-B'),
        (SELECT id FROM users WHERE username='demo_sysadmin')
 FROM (SELECT 'BUSINESS_READ' AS value UNION ALL SELECT 'INTERNAL_NOTE_READ' UNION ALL SELECT 'STATS_VIEW')
      AS seed_capabilities
 WHERE NOT EXISTS (
   SELECT 1 FROM management_grants WHERE user_id=(SELECT id FROM users WHERE username='demo_sysadmin')
     AND management_grants.capability=seed_capabilities.value
     AND scope_location_id=(SELECT id FROM locations WHERE code='DEMO-CAMPUS-B')
     AND revoked_at IS NULL);

-- Four historical, submitted example orders. No assignments or fabricated repair history.
INSERT INTO work_orders(order_no,reporter_id,category_id,location_id,title,description,
                        contact_name,contact_phone,status,created_at,updated_at,submitted_at) VALUES
 ('WX202001010001',(SELECT id FROM users WHERE username='demo_reporter01'),
  (SELECT id FROM fault_categories WHERE code='DEMO-SOCKET'),(SELECT id FROM locations WHERE code='DEMO-A1-101'),
  'Demo socket repair','Fictional socket does not work','Demo Reporter 1','00000000000','PENDING_REVIEW',
  '2020-01-01T07:55:00.000Z','2020-01-01T08:00:00.000Z','2020-01-01T08:00:00.000Z'),
 ('WX202001010002',(SELECT id FROM users WHERE username='demo_reporter02'),
  (SELECT id FROM fault_categories WHERE code='DEMO-PIPE'),(SELECT id FROM locations WHERE code='DEMO-B1-101'),
  'Demo pipe repair','Fictional pipe is leaking','Demo Reporter 2','00000000000','PENDING_REVIEW',
  '2020-01-01T07:56:00.000Z','2020-01-01T08:01:00.000Z','2020-01-01T08:01:00.000Z'),
 ('WX202001010003',(SELECT id FROM users WHERE username='demo_reporter03'),
  (SELECT id FROM fault_categories WHERE code='DEMO-WIFI'),(SELECT id FROM locations WHERE code='DEMO-A2-101'),
  'Demo network repair','Fictional network is unavailable','Demo Reporter 3','00000000000','PENDING_REVIEW',
  '2020-01-01T07:57:00.000Z','2020-01-01T08:02:00.000Z','2020-01-01T08:02:00.000Z'),
 ('WX202001010004',(SELECT id FROM users WHERE username='demo_worker01'),
  (SELECT id FROM fault_categories WHERE code='DEMO-PIPE'),(SELECT id FROM locations WHERE code='DEMO-B2-101'),
  'Demo worker self-report','Fictional worker reports own pipe issue','Demo Worker 1','00000000000',
  'PENDING_REVIEW','2020-01-01T07:58:00.000Z','2020-01-01T08:03:00.000Z','2020-01-01T08:03:00.000Z')
 ON CONFLICT(order_no) DO NOTHING;

INSERT INTO status_logs(order_id,actor_id,action,from_status,to_status,public_note,request_id,created_at)
 SELECT id,reporter_id,'ORDER_CREATED',NULL,'DRAFT','Fictional demo order created',
        'seed-w3d2-create-' || order_no,created_at
 FROM work_orders WHERE order_no IN ('WX202001010001','WX202001010002','WX202001010003','WX202001010004')
   AND NOT EXISTS (SELECT 1 FROM status_logs WHERE status_logs.order_id=work_orders.id
                   AND status_logs.action='ORDER_CREATED');

INSERT INTO status_logs(order_id,actor_id,action,from_status,to_status,public_note,request_id,created_at)
 SELECT id,reporter_id,'ORDER_SUBMITTED','DRAFT','PENDING_REVIEW','Fictional demo order',
        'seed-w3d2-' || order_no,submitted_at
 FROM work_orders WHERE order_no IN ('WX202001010001','WX202001010002','WX202001010003','WX202001010004')
   AND NOT EXISTS (SELECT 1 FROM status_logs WHERE status_logs.order_id=work_orders.id
                   AND status_logs.action='ORDER_SUBMITTED');
