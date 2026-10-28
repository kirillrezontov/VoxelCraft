# ADR 0004: camera-relative culling and a bounded terrain worker

Status: accepted.

Follow SPEC §29's optional feature order after the existing interactive slice.
Cull each uploaded terrain mesh using its actual local vertex bounds against six
planes extracted from the camera matrix. Vulkan uses zero-to-one clip Z; use row 2
for the near plane, not the OpenGL row 3 plus row 2 formula. Perform translation
and tests in double precision relative to the camera, preserving the renderer's
large-coordinate design. Use conservative plane tolerances. Report submitted draw
and triangle counts separately from resident GPU resources.

Background generation uses one bounded C++ jthread, not a generic job framework.
Requests and completed results share a capacity limit; duplicates remain pending
until consumed. The generator is copied into the worker and owns all its state.
Only the main thread owns World, input and Vulkan resources. Exceptions are
returned to the caller and destruction stops/joins before worker data is released.
Initial generation continues to process desktop events and presents a loading view.

Residency initially stays finite. Persistent edited chunks must be implemented
before eviction/streaming can safely replace this behavior. Background meshing,
GPU upload retirement and multiple workers require separate measured work.
