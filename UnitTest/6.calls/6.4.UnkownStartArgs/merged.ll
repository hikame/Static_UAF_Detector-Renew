; ModuleID = 'merged.bc'
source_filename = "llvm-link"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

%struct.evp_pkey_st = type { i8* }
%struct.evp_pkey_ctx_st = type { %struct.evp_pkey_st*, %struct.evp_pkey_st* }

; Function Attrs: noinline nounwind optnone uwtable
define void @EVP_PKEY_free(%struct.evp_pkey_st*) #0 !dbg !10 {
  %2 = alloca %struct.evp_pkey_st*, align 8
  store %struct.evp_pkey_st* %0, %struct.evp_pkey_st** %2, align 8
  call void @llvm.dbg.declare(metadata %struct.evp_pkey_st** %2, metadata !18, metadata !DIExpression()), !dbg !19
  %3 = load %struct.evp_pkey_st*, %struct.evp_pkey_st** %2, align 8, !dbg !20
  %4 = getelementptr inbounds %struct.evp_pkey_st, %struct.evp_pkey_st* %3, i32 0, i32 0, !dbg !21
  %5 = load i8*, i8** %4, align 8, !dbg !21
  call void @free(i8* %5) #3, !dbg !22
  ret void, !dbg !23
}

; Function Attrs: nounwind readnone speculatable
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

; Function Attrs: nounwind
declare void @free(i8*) #2

; Function Attrs: noinline nounwind optnone uwtable
define void @EVP_PKEY_CTX_free(%struct.evp_pkey_ctx_st*) #0 !dbg !24 {
  %2 = alloca %struct.evp_pkey_ctx_st*, align 8
  store %struct.evp_pkey_ctx_st* %0, %struct.evp_pkey_ctx_st** %2, align 8
  call void @llvm.dbg.declare(metadata %struct.evp_pkey_ctx_st** %2, metadata !33, metadata !DIExpression()), !dbg !34
  %3 = load %struct.evp_pkey_ctx_st*, %struct.evp_pkey_ctx_st** %2, align 8, !dbg !35
  %4 = icmp eq %struct.evp_pkey_ctx_st* %3, null, !dbg !37
  br i1 %4, label %5, label %6, !dbg !38

; <label>:5:                                      ; preds = %1
  br label %13, !dbg !39

; <label>:6:                                      ; preds = %1
  %7 = load %struct.evp_pkey_ctx_st*, %struct.evp_pkey_ctx_st** %2, align 8, !dbg !40
  %8 = getelementptr inbounds %struct.evp_pkey_ctx_st, %struct.evp_pkey_ctx_st* %7, i32 0, i32 0, !dbg !41
  %9 = load %struct.evp_pkey_st*, %struct.evp_pkey_st** %8, align 8, !dbg !41
  call void @EVP_PKEY_free(%struct.evp_pkey_st* %9), !dbg !42
  %10 = load %struct.evp_pkey_ctx_st*, %struct.evp_pkey_ctx_st** %2, align 8, !dbg !43
  %11 = getelementptr inbounds %struct.evp_pkey_ctx_st, %struct.evp_pkey_ctx_st* %10, i32 0, i32 1, !dbg !44
  %12 = load %struct.evp_pkey_st*, %struct.evp_pkey_st** %11, align 8, !dbg !44
  call void @EVP_PKEY_free(%struct.evp_pkey_st* %12), !dbg !45
  br label %13, !dbg !46

; <label>:13:                                     ; preds = %6, %5
  ret void, !dbg !46
}

; Function Attrs: noinline nounwind optnone uwtable
define i32 @main() #0 !dbg !47 {
  call void @EVP_PKEY_CTX_free(%struct.evp_pkey_ctx_st* null), !dbg !51
  ret i32 0, !dbg !52
}

attributes #0 = { noinline nounwind optnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind readnone speculatable }
attributes #2 = { nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #3 = { nounwind }

!llvm.dbg.cu = !{!0}
!llvm.ident = !{!5}
!llvm.module.flags = !{!6, !7, !8, !9}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "clang version 6.0.0-1ubuntu2 (tags/RELEASE_600/final)", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, enums: !2, retainedTypes: !3)
!1 = !DIFile(filename: "foo.c", directory: "/home/kame/workspace/Static_UAF_Detector-Renew/UnitTest/6.calls/6.4.UnkownStartArgs")
!2 = !{}
!3 = !{!4}
!4 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: null, size: 64)
!5 = !{!"clang version 6.0.0-1ubuntu2 (tags/RELEASE_600/final)"}
!6 = !{i32 2, !"Dwarf Version", i32 4}
!7 = !{i32 2, !"Debug Info Version", i32 3}
!8 = !{i32 1, !"wchar_size", i32 4}
!9 = !{i32 7, !"PIC Level", i32 2}
!10 = distinct !DISubprogram(name: "EVP_PKEY_free", scope: !1, file: !1, line: 18, type: !11, isLocal: false, isDefinition: true, scopeLine: 18, flags: DIFlagPrototyped, isOptimized: false, unit: !0, variables: !2)
!11 = !DISubroutineType(types: !12)
!12 = !{null, !13}
!13 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !14, size: 64)
!14 = !DIDerivedType(tag: DW_TAG_typedef, name: "EVP_PKEY", file: !1, line: 7, baseType: !15)
!15 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "evp_pkey_st", file: !1, line: 9, size: 64, elements: !16)
!16 = !{!17}
!17 = !DIDerivedType(tag: DW_TAG_member, name: "lock", scope: !15, file: !1, line: 10, baseType: !4, size: 64)
!18 = !DILocalVariable(name: "x", arg: 1, scope: !10, file: !1, line: 18, type: !13)
!19 = !DILocation(line: 18, column: 30, scope: !10)
!20 = !DILocation(line: 19, column: 10, scope: !10)
!21 = !DILocation(line: 19, column: 13, scope: !10)
!22 = !DILocation(line: 19, column: 5, scope: !10)
!23 = !DILocation(line: 21, column: 1, scope: !10)
!24 = distinct !DISubprogram(name: "EVP_PKEY_CTX_free", scope: !1, file: !1, line: 23, type: !25, isLocal: false, isDefinition: true, scopeLine: 24, flags: DIFlagPrototyped, isOptimized: false, unit: !0, variables: !2)
!25 = !DISubroutineType(types: !26)
!26 = !{null, !27}
!27 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !28, size: 64)
!28 = !DIDerivedType(tag: DW_TAG_typedef, name: "EVP_PKEY_CTX", file: !1, line: 6, baseType: !29)
!29 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "evp_pkey_ctx_st", file: !1, line: 13, size: 128, elements: !30)
!30 = !{!31, !32}
!31 = !DIDerivedType(tag: DW_TAG_member, name: "pkey", scope: !29, file: !1, line: 14, baseType: !13, size: 64)
!32 = !DIDerivedType(tag: DW_TAG_member, name: "peerkey", scope: !29, file: !1, line: 15, baseType: !13, size: 64, offset: 64)
!33 = !DILocalVariable(name: "ctx", arg: 1, scope: !24, file: !1, line: 23, type: !27)
!34 = !DILocation(line: 23, column: 38, scope: !24)
!35 = !DILocation(line: 25, column: 9, scope: !36)
!36 = distinct !DILexicalBlock(scope: !24, file: !1, line: 25, column: 9)
!37 = !DILocation(line: 25, column: 13, scope: !36)
!38 = !DILocation(line: 25, column: 9, scope: !24)
!39 = !DILocation(line: 26, column: 9, scope: !36)
!40 = !DILocation(line: 27, column: 19, scope: !24)
!41 = !DILocation(line: 27, column: 24, scope: !24)
!42 = !DILocation(line: 27, column: 5, scope: !24)
!43 = !DILocation(line: 28, column: 19, scope: !24)
!44 = !DILocation(line: 28, column: 24, scope: !24)
!45 = !DILocation(line: 28, column: 5, scope: !24)
!46 = !DILocation(line: 29, column: 1, scope: !24)
!47 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 31, type: !48, isLocal: false, isDefinition: true, scopeLine: 31, isOptimized: false, unit: !0, variables: !2)
!48 = !DISubroutineType(types: !49)
!49 = !{!50}
!50 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!51 = !DILocation(line: 32, column: 3, scope: !47)
!52 = !DILocation(line: 33, column: 1, scope: !47)
