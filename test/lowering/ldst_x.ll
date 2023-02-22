; RUN: %opt -passes=lower-amx %s -o - -S | FileCheck %s

; CHECK:   [[ADDR:%.*]] = ptrtoint ptr %0 to i64
; CHECK:   [[OR1:%.*]] = or i64 [[ADDR]], 504403158265495552
; CHECK:   call void asm sideeffect ".word (0x201000 + ($0 << 5) + 0$1 - ((0$1 >> 4) * 6))", "i,r,~{memory}"(i32 0, i64 [[OR1]])
; CHECK:   [[ADDR2:%.*]] = ptrtoint ptr %1 to i64
; CHECK:   [[OR2:%.*]] = or i64 [[ADDR2]], 504403158265495552
; CHECK:   call void asm sideeffect ".word (0x201000 + ($0 << 5) + 0$1 - ((0$1 >> 4) * 6))", "i,r,~{memory}"(i32 2, i64 [[OR2]])
; CHECK:   ret void

target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

; Function Attrs: nounwind ssp uwtable
define void @amx_copy(ptr noundef %0, ptr noundef %1) local_unnamed_addr #0 {
  tail call void @amx_ldx(i32 noundef 7, ptr noundef %0) #2
  tail call void @amx_stx(ptr noundef %1, i32 noundef 7) #2
  ret void
}

declare void @amx_ldx(i32 noundef, ptr noundef) local_unnamed_addr #1

declare void @amx_stx(ptr noundef, i32 noundef) local_unnamed_addr #1

attributes #0 = { nounwind ssp uwtable "frame-pointer"="non-leaf" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="apple-m1" "target-features"="+aes,+crc,+crypto,+dotprod,+fp-armv8,+fp16fml,+fullfp16,+lse,+neon,+ras,+rcpc,+rdm,+sha2,+sha3,+sm4,+v8.5a,+zcm,+zcz" }
attributes #1 = { "frame-pointer"="non-leaf" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="apple-m1" "target-features"="+aes,+crc,+crypto,+dotprod,+fp-armv8,+fp16fml,+fullfp16,+lse,+neon,+ras,+rcpc,+rdm,+sha2,+sha3,+sm4,+v8.5a,+zcm,+zcz" }
attributes #2 = { nounwind }

!llvm.module.flags = !{!0, !1, !2, !3}
!llvm.ident = !{!4}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"PIC Level", i32 2}
!2 = !{i32 7, !"uwtable", i32 2}
!3 = !{i32 7, !"frame-pointer", i32 1}
!4 = !{!"clang version 15.0.7 (https://github.com/llvm/llvm-project.git 8dfdcc7b7bf66834a761bd8de445840ef68e4d1a)"}
!5 = !{i64 2148188005, i64 2148188011, i64 2148188016, i64 2148188021}
!6 = !{i64 2148188125, i64 2148188131, i64 2148188136, i64 2148188141}
