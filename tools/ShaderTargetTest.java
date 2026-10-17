package com.axuan.vivomiuix;
import java.lang.reflect.*;
public final class ShaderTargetTest {
 static final class Renamed { private Renamed arbitraryName(boolean force){return this;} static Renamed decoy(boolean force){return null;} }
 static final class Missing { Missing unrelated(int value){return this;} }
 static final class Ambiguous { Ambiguous a(boolean value){return this;} Ambiguous b(boolean value){return this;} }
 public static void main(String[] args)throws Exception {
  Method method=ShaderContracts.target(Renamed.class);
  if(!method.getName().equals("arbitraryName")||Modifier.isStatic(method.getModifiers()))throw new AssertionError("Renamed target was not selected");
  for(Class<?> type:new Class<?>[]{Missing.class,Ambiguous.class}) {
   try {ShaderContracts.target(type);throw new AssertionError("Unsupported target accepted: "+type);}
   catch(NoSuchMethodException expected) { }
  }
  System.out.println("PASS renamed target, static decoy exclusion, missing and ambiguous contracts");
 }
}
