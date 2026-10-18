"""Host lifecycle tests of DeferredStartup with bootstrap Android queue stubs."""
from pathlib import Path
import subprocess,json
root=Path(__file__).resolve().parents[1]
out=root/'build/host-startup';out.mkdir(parents=True,exist_ok=True)
java=Path('C:/Program Files/Java/jdk-21.0.9+10/bin')
stubs={
'android/os/Looper.java':'''package android.os;
public class Looper {public static volatile Looper main;public static Looper getMainLooper(){return main;}}
''',
'android/os/Handler.java':'''package android.os;
import java.util.concurrent.*;
public class Handler {
 public static volatile boolean reject;
 public static final DelayQueue<Entry> queue=new DelayQueue<>();
 public static class Entry implements Delayed {final Runnable task;final long due;
  Entry(Runnable task,long delay){this.task=task;due=System.nanoTime()+delay*1000000;}
  public long getDelay(TimeUnit u){return u.convert(due-System.nanoTime(),TimeUnit.NANOSECONDS);}
  public int compareTo(Delayed other){return Long.compare(due,((Entry)other).due);}
  public void run(){task.run();}}
 public Handler(Looper looper){}
 public boolean post(Runnable task){return postDelayed(task,0);}
 public boolean postDelayed(Runnable task,long delay){if(reject)return false;queue.offer(new Entry(task,delay));return true;}
}
''',
'android/app/ActivityThread.java':'''package android.app;
public class ActivityThread {public static volatile Object app;public static Object currentApplication(){return app;}}
''',
'android/app/Application.java':'package android.app; public class Application {}',
'org/lsposed/manager/App.java':'package org.lsposed.manager; public class App extends android.app.Application {}',
}
files=[]
for name,text in stubs.items():
 p=out/'stubs'/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(text,'utf8');files.append(p)
boot=out/'boot';boot.mkdir(exist_ok=True)
subprocess.run([str(java/'javac.exe'),'--release','8','-Xlint:-options','-d',str(boot),*[str(p) for p in files]],check=True)
test=out/'src/com/axuan/vivomiuix/DeferredStartupTest.java';test.parent.mkdir(parents=True,exist_ok=True)
test.write_text('''package com.axuan.vivomiuix;
import android.os.*;import android.app.*;import java.util.concurrent.*;
public class DeferredStartupTest {
 static volatile int calls;static volatile String report;
 static void check(boolean v,String m){if(!v)throw new AssertionError(m);}
 public static void main(String[] args)throws Exception {
  final String mode=args[0];Looper.main=mode.equals("looper-delay")?null:new Looper();
  ActivityThread.app=mode.equals("ordinary")?new Application():mode.equals("after-bind")||mode.equals("timeout")?null:new org.lsposed.manager.App();
  Handler.reject=mode.equals("reject");
  DeferredStartup.Callbacks cb=new DeferredStartup.Callbacks(){
   public boolean initialize(){check(ActivityThread.app instanceof org.lsposed.manager.App,"application not ready");calls++;return !mode.equals("refuse");}
   public void report(String message){report=message;}
  };
  DeferredStartup.start(cb);DeferredStartup.start(cb);
  if(mode.equals("looper-delay")){Thread.sleep(80);check(calls==0,"early Looper");Looper.main=new Looper();}
  long deadline=System.nanoTime()+7000000000L;boolean first=true;
  while(System.nanoTime()<deadline&&calls==0&&report==null){
   Handler.Entry task=Handler.queue.poll(100,TimeUnit.MILLISECONDS);
   if(task!=null){task.run();if(first&&mode.equals("after-bind")){check(calls==0,"early Application");ActivityThread.app=new org.lsposed.manager.App();}first=false;}
  }
  if(mode.equals("ordinary")||mode.equals("timeout")||mode.equals("reject"))check(calls==0&&report!=null,"refusal did not finish");
  else check(calls==1,"initializer count");
  if(mode.equals("timeout"))check(report.contains("timed out"),"timeout missing");
  if(mode.equals("refuse"))check(report!=null&&report.contains("refused"),"native failure lost");
  Thread.sleep(80);check(Handler.queue.isEmpty(),"persistent retry remains");
  for(Thread t:Thread.getAllStackTraces().keySet())check(!t.getName().equals("VivoMiuixShaderInit"),"worker remains");
  System.out.println("PASS "+mode);
 }
}''','utf8')
classes=out/'classes';classes.mkdir(exist_ok=True)
subprocess.run([str(java/'javac.exe'),'--release','8','-Xlint:-options','-cp',str(boot),'-d',str(classes),str(root/'src/DeferredStartup.java'),str(test)],check=True)
results=[]
for mode in ['ready','after-bind','looper-delay','ordinary','refuse','reject','timeout']:
 run=subprocess.run([str(java/'java.exe'),'-Xbootclasspath/a:'+str(boot),'-cp',str(classes),'com.axuan.vivomiuix.DeferredStartupTest',mode],check=True,capture_output=True,text=True,timeout=10)
 print(run.stdout.strip());results.append(run.stdout.strip())
(out/'results.json').write_text(json.dumps({'scope':'JDK queue and Application stubs; no Android ART validation','results':results},indent=2),'utf8')
