import tkinter as tk
from tkinter import messagebox

# Função para corrigir a resposta
def verificar_resposta():

    # Verifica se o usuário escolheu alguma alternativa
    if resposta.get() == "":
        messagebox.showwarning(
            "Atenção",
            "Selecione uma alternativa!"
        )
        return

    # Processamento da resposta
    if resposta.get() == "Python":
        resultado.config(
            text="✅ Correto! Python é a resposta certa."
        )
    else:
        resultado.config(
            text="❌ Resposta incorreta."
        )


# Função para limpar a tela
def limpar():
    resposta.set("")
    resultado.config(text="")


# Criação da janela principal
janela = tk.Tk()

# Título da janela
janela.title("Quiz Desafio Tech")

# Tamanho da janela
janela.geometry("500x350")


# Título do sistema
titulo = tk.Label(
    janela,
    text="Quiz de Programação",
    font=("Arial", 16, "bold")
)
titulo.pack(pady=10)


# Pergunta
pergunta = tk.Label(
    janela,
    text="Qual linguagem de programação é utilizada neste projeto?"
)
pergunta.pack(pady=10)


# Variável que armazenará a resposta
resposta = tk.StringVar()


# Alternativas utilizando Radiobutton
rb1 = tk.Radiobutton(
    janela,
    text="Java",
    variable=resposta,
    value="Java"
)
rb1.pack()

rb2 = tk.Radiobutton(
    janela,
    text="Python",
    variable=resposta,
    value="Python"
)
rb2.pack()

rb3 = tk.Radiobutton(
    janela,
    text="C++",
    variable=resposta,
    value="C++"
)
rb3.pack()


# Botão para verificar resposta
btn_verificar = tk.Button(
    janela,
    text="Verificar Resposta",
    command=verificar_resposta
)
btn_verificar.pack(pady=15)


# Botão para limpar
btn_limpar = tk.Button(
    janela,
    text="Limpar",
    command=limpar
)
btn_limpar.pack()


# Label onde será exibido o resultado
resultado = tk.Label(
    janela,
    text="",
    font=("Arial", 12)
)
resultado.pack(pady=20)


# Mantém a janela aberta
janela.mainloop()