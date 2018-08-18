

lines before the first line starting in @ are ignored


@0
mov l8 in, r0 # r0 = x
mov l8 in, r1 # r1 = y
  # check for negative input
  mov l8 in r2 # r2 = xx
  jeq l8 r2 99999999 label
  jmp label # test
  # continue input /
a:# delta computation
b:sub l8 r0 r2 # r2 = dx
  mov l8 in r3 # r3 = yy
  sub l8 r1 r3 # r3 = dy
  # compute distSq
  mul l8 r2 r2
  mul l8 r3 r3
  add l8 r3 r2 # r2 = distSq
  # send to storage node
  mov l8 r2 left ?
  mov l8 in right
  mov l8 in left
label:mov:,




@1
wait left
mov l8 left r0 # distSq
mov l8 right r1 # TODO
mov l8 left r2 # TODO