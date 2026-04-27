# Exemplos
func:hello({
    printf("Hello {name}!")
})(string:name("World")) returns void

func:main({
    loop(integer:i(0); i < 10; i += 1) {
        printf(hello(i))
    }
}) returns void

fn : fatorial({
  // conditional | then | else
  if(num in(0,1), num, num * fatorial(set:num(num-1)))
})(integer:num) returns integer

fatorial(arg:num(5)) -> {
  println("5! = <:num>")
}

fn : teste({
  if(idade < 18, "jovem", 
    if(idade >=18 and idade < 30, "jovem-adulto", 
      if(idade >= 30 and idade < 60, "adulto", "idoso")
    )
  )
})(integer:idade) returns string

// ou

fn:teste({
    cond(
        idade < 18,                       "jovem",
        idade >= 18 and idade < 30,       "jovem-adulto",
        idade >= 30 and idade < 60,       "adulto",
        "idoso"
    )
})(integer:idade) returns string

## Tipos
  **Array**: array<integer>:arrnums([1,2,3,4,5]) // fixo em memoria
  arrnums.push(10) // panic, array nao eh dinamico, eh constante
  **Vector**: vector<integer>:vecnums([1,2,3]) // dinamico
  vecnums.push(10) -> [1,2,3,10]
  vector<integer>:evennums(
    vecnums |> push(10) |> filter(n -> n % 2 == 0)
  )
