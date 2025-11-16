Urban Jarvis
Urban Jarvis é um sistema inteligente para pontos de ônibus desenvolvido com ESP32, projetado para trazer informações de transporte público em tempo real aos cidadãos de Curitiba. Utilizando APIs oficiais da URBS e dados municipais abertos, o dispositivo entrega funcionalidades como busca de horários, previsão de chegada e ordenação eficiente dos dados usando estruturas de dados típicas de Estrutura de Dados II, como árvores binárias.

Sobre o projeto
O Urban Jarvis conecta-se automaticamente ao Wi-Fi disponível, acessa a API da URBS para consultar horários de ônibus, e exibe em tela as informações de chegada mais relevantes para o usuário.

Em caso de ausência de internet, o sistema opera com dados em cache local, garantindo funcionamento contínuo.

O processamento dos dados (busca pelo próximo horário, organização dos horários exibidos) é realizado alinhando teoria e prática de Estrutura de Dados, facilitando consultas rápidas por meio de árvores binárias.

O hardware é baseado em ESP32 S3, com display TFT sensível ao toque e controle visual por LED, tornando a interface mais acessível e interativa.

ESP32 e estrutura de dados
O ESP32 é um microcontrolador de alto desempenho, com Wi-Fi, Bluetooth, múltiplos GPIOs e ideal para aplicações IoT em ambientes urbanos. Sua capacidade de processamento e conectividade facilita o uso de algoritmos eficientes e estruturas de dados como BST (Árvore de Busca Binária) e cache offline, essenciais para sistemas dinâmicos de cidades inteligentes e aplicações de acessibilidade e mobilidade urbana.​

Equipe
Projeto realizado por estudantes do Centro Universitário UniBrasil:

João Vitor Soares da Silva

Matheus Bilro Pereira Leite

Leonardo Bora da Costa

Luan Constancio
