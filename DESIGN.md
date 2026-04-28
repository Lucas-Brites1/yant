### Functions
fn:identifier(integer:X, integer:Y, integer:Z {...}) ret:integer
integer:someNumber(identifier(:X 30, :Y 40, :Z 50))


fn:hello(string:name {
  printf("Hello {name}!")
}) ret:nil

hello(:name "Lucas")
hello("Lucas")
