import sys,threading,time,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from server import WorkQueue
class SimulatedConsole:
 def __init__(self):self.roles={i:{} for i in range(1,7)};self.calls=[];self.hold=threading.Event();self.hold.set();self.fail_role=0
 def _port(self,p):pass
 def inspect(self,p,progress):self.hold.wait(2);self.calls.append(p);return {'name':p}
 def execute(self,action,port,mac,role,progress,replace):
  self.hold.wait(2);self.calls.append(role);progress('回读中')
  if role==self.fail_role:raise ValueError('模拟回读失败')
  return {'role':role,'verified':True}
class QueueTests(unittest.TestCase):
 def setUp(self):self.console=SimulatedConsole();self.q=WorkQueue(self.console)
 def wait_done(self):
  deadline=time.monotonic()+3
  while time.monotonic()<deadline:
   if self.q.snapshot()['done']:return self.q.snapshot()
   threading.Event().wait(.01)
  self.fail('queue did not finish')
 def targets(self):return [{'port':f'/dev/cu.usbmodem{i}','mac':str(i),'role':i} for i in range(1,7)]
 def test_six_devices_finish_independently_and_failure_is_not_success(self):
  self.console.fail_role=3;self.q.start('restore',self.targets());job=self.wait_done()
  self.assertEqual(sorted(self.console.calls),[1,2,3,4,5,6]);self.assertEqual([i['status'] for i in job['items']],['success','success','error','success','success','success'])
  self.assertIn('模拟回读失败',job['items'][2]['stage'])
 def test_busy_job_cannot_be_overwritten(self):
  self.console.hold.clear();self.addCleanup(self.console.hold.set);self.q.start('inspect',[{'port':'/dev/cu.usbmodem1'}])
  with self.assertRaises(RuntimeError):self.q.start('inspect',[{'port':'/dev/cu.usbmodem2'}])
  self.console.hold.set();self.wait_done();self.assertEqual(self.console.calls,['/dev/cu.usbmodem1'])
 def test_batch_duplicate_roles_or_ports_are_rejected_before_dispatch(self):
  targets=self.targets();targets[1]['role']=1
  with self.assertRaises(ValueError):self.q.start('restore',targets)
  targets=self.targets();targets[1]['port']=targets[0]['port']
  with self.assertRaises(ValueError):self.q.start('restore',targets)
  self.assertEqual(self.console.calls,[]);self.assertIsNone(self.q.snapshot())
if __name__=='__main__':unittest.main()
