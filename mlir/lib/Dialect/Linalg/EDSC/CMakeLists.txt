add_mlir_dialect_library(MLIRLinalgEDSC
  Builders.cpp

  ADDITIONAL_HEADER_DIRS
  ${MLIR_MAIN_INCLUDE_DIR}/mlir/Dialect/Linalg

  LINK_LIBS PUBLIC
  MLIREDSC
  MLIRIR
  MLIRAffineOps
  MLIRAffineEDSC
  MLIRLinalgOps
  MLIRLoopOps
  MLIRStandardOps
  )
