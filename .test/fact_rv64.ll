; ModuleID = 'mod'
source_filename = "mod"
target datalayout = "e-m:e-p:64:64-i64:64-i128:128-n32:64-S128"

@instrew_baseaddr = external global i64, !absolute_symbol !0
@llvm.used = appending global [2 x ptr] [ptr @instrew_baseaddr, ptr @syscall_rv64], section "llvm.metadata"

declare void @syscall_rv64(ptr addrspace(1))

; Function Attrs: null_pointer_is_valid
define void @S0_10144(ptr addrspace(1) noalias nocapture align 16 dereferenceable(400) %0) #0 {
  br label %2

2:                                                ; preds = %1
  %3 = getelementptr inbounds i8, ptr addrspace(1) %0, i64 88
  %4 = getelementptr inbounds i8, ptr addrspace(1) %0, i64 16
  store i64 65866, ptr addrspace(1) %4, align 8
  store i64 5, ptr addrspace(1) %3, align 8
  br label %5

5:                                                ; preds = %2
  store i64 65880, ptr addrspace(1) %0, align 8
  ret void
}

attributes #0 = { null_pointer_is_valid }

!0 = !{i64 -1, i64 -1}
; ModuleID = 'mod'
source_filename = "mod"
target datalayout = "e-m:e-p:64:64-i64:64-i128:128-n32:64-S128"

@instrew_baseaddr = external global i64, !absolute_symbol !0
@llvm.used = appending global [2 x ptr] [ptr @instrew_baseaddr, ptr @syscall_rv64], section "llvm.metadata"

declare void @syscall_rv64(ptr addrspace(1))

; Function Attrs: null_pointer_is_valid
define void @S0_10158(ptr addrspace(1) noalias nocapture align 16 dereferenceable(400) %0) #0 {
  %2 = getelementptr inbounds i8, ptr addrspace(1) %0, i64 16
  %3 = getelementptr inbounds i8, ptr addrspace(1) %0, i64 88
  %4 = load i64, ptr addrspace(1) %3, align 8
  %5 = load i64, ptr addrspace(1) %2, align 8
  %6 = getelementptr inbounds i8, ptr addrspace(1) %0, i64 128
  %7 = load i64, ptr addrspace(1) %6, align 8
  %8 = getelementptr inbounds i8, ptr addrspace(1) %0, i64 120
  %9 = load i64, ptr addrspace(1) %8, align 8
  %10 = icmp slt i64 %4, 2
  br i1 %10, label %13, label %21

11:                                               ; preds = %22
  %12 = add i64 %25, 1
  br label %13

13:                                               ; preds = %11, %1
  %14 = phi i64 [ %7, %1 ], [ %25, %11 ]
  %15 = phi i64 [ %9, %1 ], [ %12, %11 ]
  %16 = phi i64 [ %4, %1 ], [ %26, %11 ]
  %17 = getelementptr inbounds i8, ptr addrspace(1) %0, i64 88
  store i64 %16, ptr addrspace(1) %17, align 8
  %18 = getelementptr inbounds i8, ptr addrspace(1) %0, i64 96
  store i64 1, ptr addrspace(1) %18, align 8
  %19 = getelementptr inbounds i8, ptr addrspace(1) %0, i64 120
  store i64 %15, ptr addrspace(1) %19, align 8
  %20 = getelementptr inbounds i8, ptr addrspace(1) %0, i64 128
  store i64 %14, ptr addrspace(1) %20, align 8
  store i64 %5, ptr addrspace(1) %0, align 8
  ret void

21:                                               ; preds = %1
  br label %22

22:                                               ; preds = %22, %21
  %23 = phi i64 [ 1, %21 ], [ %26, %22 ]
  %24 = phi i64 [ %4, %21 ], [ %25, %22 ]
  %25 = add i64 %24, -1
  %26 = mul i64 %24, %23
  %27 = icmp eq i64 %25, 1
  br i1 %27, label %11, label %22
}

attributes #0 = { null_pointer_is_valid }

!0 = !{i64 -1, i64 -1}

