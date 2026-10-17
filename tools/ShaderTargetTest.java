package com.axuan.vivomiuix;
import java.lang.reflect.*;
public final class ShaderTargetTest {
 static final class Renamed { private Renamed arbitraryName(boolean force){return this;} static Renamed decoy(boolean force){return null;} }
 static final class Missing { Missing unrelated(int value){return this;} }
 static final class Ambiguous { Ambiguous a(boolean value){return this;} Ambiguous b(boolean value){return this;} }
 static abstract class Controller {
  void renamedCaller(){} static void anotherCaller(){}
  native void nativeDecoy(); abstract void abstractDecoy();
 }
 public static void main(String[] args)throws Exception {
  Method method=ShaderContracts.target(Renamed.class);
  if(!method.getName().equals("arbitraryName")||Modifier.isStatic(method.getModifiers()))throw new AssertionError("Renamed target was not selected");
  for(Class<?> type:new Class<?>[]{Missing.class,Ambiguous.class}) {
   try {ShaderContracts.target(type);throw new AssertionError("Unsupported target accepted: "+type);}
   catch(NoSuchMethodException expected) { }
  }
  java.util.List<Member> callers=ShaderContracts.runtimeCallers(Renamed.class,Controller.class);
  if(callers.size()!=4)throw new AssertionError("Unexpected bounded caller count: "+callers);
  for(Member m:callers)if(Modifier.isNative(m.getModifiers())||Modifier.isAbstract(m.getModifiers()))
   throw new AssertionError("Unsupported caller included");
  for(Class<?> c:new Class<?>[]{RuntimeLayoutProbe.class,RuntimeLayoutPeer.class}) {
   for(String n:new String[]{"first","second"}) {
    Method m=c.getDeclaredMethod(n,int.class);
    if(!Modifier.isNative(m.getModifiers())||Modifier.isStatic(m.getModifiers())||m.getReturnType()!=int.class)
     throw new AssertionError("JNI witness must use instance invocation");
   }
   c.getConstructor().newInstance();
  }
  System.out.println("PASS target shape and refusal, bounded reflection callers, native/abstract exclusion, instance JNI witness ABI");
 }
}
