; ModuleID = 'LoopFusion.mem2reg.bc'
source_filename = "LoopFusion.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx14.0.0"

@__const.main.b = private unnamed_addr constant [5 x i32] [i32 1, i32 2, i32 3, i32 4, i32 5], align 4
@__const.main.c = private unnamed_addr constant [5 x i32] [i32 6, i32 7, i32 8, i32 9, i32 0], align 4

define void @loop_fusion(ptr noundef %0, ptr noundef %1, ptr noundef %2, ptr noundef %3) {
  br label %5

5:                                                ; preds = %18, %4
  %.01 = phi i32 [ 0, %4 ], [ %19, %18 ]
  %6 = icmp slt i32 %.01, 5
  br i1 %6, label %7, label %20

7:                                                ; preds = %5
  %8 = sext i32 %.01 to i64
  %9 = getelementptr inbounds i32, ptr %1, i64 %8
  %10 = load i32, ptr %9, align 4
  %11 = sdiv i32 1, %10
  %12 = sext i32 %.01 to i64
  %13 = getelementptr inbounds i32, ptr %2, i64 %12
  %14 = load i32, ptr %13, align 4
  %15 = mul nsw i32 %11, %14
  %16 = sext i32 %.01 to i64
  %17 = getelementptr inbounds i32, ptr %0, i64 %16
  store i32 %15, ptr %17, align 4
  br label %18

18:                                               ; preds = %7
  %19 = add nsw i32 %.01, 1
  br label %5, !llvm.loop !5

20:                                               ; preds = %5
  br label %21

21:                                               ; preds = %33, %20
  %.0 = phi i32 [ 0, %20 ], [ %34, %33 ]
  %22 = icmp slt i32 %.0, 5
  br i1 %22, label %23, label %35

23:                                               ; preds = %21
  %24 = sext i32 %.0 to i64
  %25 = getelementptr inbounds i32, ptr %0, i64 %24
  %26 = load i32, ptr %25, align 4
  %27 = sext i32 %.0 to i64
  %28 = getelementptr inbounds i32, ptr %2, i64 %27
  %29 = load i32, ptr %28, align 4
  %30 = add nsw i32 %26, %29
  %31 = sext i32 %.0 to i64
  %32 = getelementptr inbounds i32, ptr %3, i64 %31
  store i32 %30, ptr %32, align 4
  br label %33

33:                                               ; preds = %23
  %34 = add nsw i32 %.0, 1
  br label %21, !llvm.loop !7

35:                                               ; preds = %21
  ret void
}

define i32 @main() {
  %1 = alloca [5 x i32], align 4
  %2 = alloca [5 x i32], align 4
  %3 = alloca [5 x i32], align 4
  %4 = alloca [5 x i32], align 4
  call void @llvm.memcpy.p0.p0.i64(ptr align 4 %2, ptr align 4 @__const.main.b, i64 20, i1 false)
  call void @llvm.memcpy.p0.p0.i64(ptr align 4 %3, ptr align 4 @__const.main.c, i64 20, i1 false)
  %5 = getelementptr inbounds [5 x i32], ptr %1, i64 0, i64 0
  %6 = getelementptr inbounds [5 x i32], ptr %2, i64 0, i64 0
  %7 = getelementptr inbounds [5 x i32], ptr %3, i64 0, i64 0
  %8 = getelementptr inbounds [5 x i32], ptr %4, i64 0, i64 0
  call void @loop_fusion(ptr noundef %5, ptr noundef %6, ptr noundef %7, ptr noundef %8)
  ret i32 0
}

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias nocapture writeonly, ptr noalias nocapture readonly, i64, i1 immarg) #0

attributes #0 = { nocallback nofree nounwind willreturn memory(argmem: readwrite) }

!llvm.module.flags = !{!0, !1, !2, !3}
!llvm.ident = !{!4}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"uwtable", i32 1}
!3 = !{i32 7, !"frame-pointer", i32 1}
!4 = !{!"clang version 17.0.6"}
!5 = distinct !{!5, !6}
!6 = !{!"llvm.loop.mustprogress"}
!7 = distinct !{!7, !6}
