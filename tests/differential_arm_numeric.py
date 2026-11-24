"""Numeric comparisons with the original Symbian ARM driver.

These ARM-mode entrypoints take scalar/pointer arguments in r0-r3 and then
stack words. All selected arguments are 32-bit; no 64-bit alignment holes
are needed. Matrices and vector records match the x86 reference layout.
"""

from differential_numeric import (
    FUNCTIONS, check_scalars, check_vector_length, check_matrices, check_vectors,
)


# Exact function boundaries from the original ARM disassembly.
ARM_FUNCTIONS = {
    "hgClz32": (0x3224C, 0x32288),
    "F_ADD": (0x32E10, 0x32EF8),
    "F_MUL": (0x334B0, 0x3351C),
    "F_SQR": (0x33728, 0x33774),
    "F_ITOF": (0x337EC, 0x3382C),
    "F_RCP": (0x33A4C, 0x33AE0),
    "F_RSQ": (0x33D00, 0x33D90),
    "F_SIN": (0x34030, 0x34050),
    "F_COS": (0x34050, 0x34064),
    "radiansToUnitScalar": (0x33F8C, 0x33FF8),
    "IUnitScalar_sin": (0x33E1C, 0x33ED0),
    "IUnitScalar_cos": (0x33ED0, 0x33F8C),
    "IUnitScalar_lerp": (0x322D4, 0x32310),
    "IUnitScalar_tos": (0x32310, 0x32348),
    "IScalar_fixedToScalar": (0x32094, 0x320EC),
    "IScalar_scalarToFixed": (0x320EC, 0x32134),
    "IScalar_byteToScalar": (0x321D0, 0x3221C),
    "IScalar_toFixedHO": (0x3221C, 0x3224C),
    "XScalar_log2": (0x31E08, 0x31F3C),
    "XScalar_exp2": (0x31F3C, 0x32094),
    "IVector3_length": (0x32514, 0x32608),
    "IMatrix4x4_setfv": (0x2D87C, 0x2D900),
    "IMatrix4x4_getfv": (0x2D900, 0x2D96C),
    "IMatrix4x4_inverse": (0x3043C, 0x30ED4),
    "IMatrix4x4_det2h": (0x3030C, 0x3043C),
    "IMatrix4x4_mul": (0x2DE4C, 0x2DEA0),
    "IMatrix4x4_transpose": (0x2D7AC, 0x2D87C),
    "IMatrix4x4_inversetranspose": (0x30EDC, 0x311B4),
    "IMatrix4x4_frustum": (0x2DED0, 0x2E098),
    "IMatrix4x4_ortho": (0x2FCA0, 0x2FE24),
    "IMatrix4x4_rotate": (0x2FFA4, 0x3030C),
    "IMatrix4x4_scale": (0x2FE24, 0x2FE84),
    "IMatrix4x4_translate": (0x2FE84, 0x2FEF0),
    "IMatrix4x4_mulfv4": (0x2FB94, 0x2FCA0),
    "IVertexTransform_copy": (0x2E85C, 0x2EA18),
    "IVertexTransform_zmulxv4": (0x2F62C, 0x2F7C8),
    "IVertexTransform_mulxv4": (0x2EB08, 0x2F2B8),
    "IVertexTransform_mulxv4s": (0x313B8, 0x315B4),
    "IVertexTransform_mulfv4": (0x2F2B8, 0x2F580),
    "IVertexTransform_mulfv4s": (0x311B4, 0x313B8),
    "IVertexTransform_zmulfv4": (0x2F584, 0x2F62C),
    "IFVector4_setfv": (0x32BF0, 0x32C94),
    "IFVector4_setxv": (0x32B5C, 0x32BF0),
    "IFVector4_tov3": (0x3197C, 0x31BA4),
    "IFVector4_dp4l": (0x2F7C8, 0x2F974),
    "IVector4_add": (0x32A68, 0x32B5C),
    "IVector4_smul": (0x32948, 0x32A68),
    "IVector3_normsub": (0x32348, 0x32514),
    "XNormal3_normalize": (0x31C98, 0x31D84),
    "XNormal3_dot": (0x31C40, 0x31C98),
}


def run(suite):
    functions = [(name, *ARM_FUNCTIONS[name], signature, result, family)
                 for name, _, _, signature, result, family in FUNCTIONS
                 if name in ARM_FUNCTIONS]
    check_scalars(suite, functions)
    check_vector_length(suite, ARM_FUNCTIONS)
    check_matrices(suite, ARM_FUNCTIONS)
    check_vectors(suite, ARM_FUNCTIONS)
