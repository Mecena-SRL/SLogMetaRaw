<p align="center"><img src="assets/icon_1024.png" width="140" alt="Log MetaRaw"></p>

<h1 align="center">Log MetaRaw</h1>
<p align="center"><b>Metadados log de câmeras (Sony, Panasonic, Apple, DJI e outras) e controles de revelação em luz de cena para o DaVinci Resolve 20 e 21</b><br>
Ivan Mazzone + Claude · <a href="https://github.com/ivan-94m">github.com/ivan-94m</a> · <a href="https://instagram.com/ivan_94m">@ivan_94m</a></p>

<p align="center">
<a href="README.md">English</a> · <a href="README.it.md">Italiano</a> · <a href="README.es.md">Español</a> · <b>Português</b> · <a href="README.zh.md">简体中文</a>
</p>

---

## O que é

As câmeras Sony gravam em cada arquivo como o plano foi filmado: balanço de branco em Kelvin, tint, EI, objetiva,
abertura, obturador, perfil de cor. O Resolve usa esses dados nos MXF da FX6 e da FX9, que ganham o painel *Camera Raw*.
Nos MP4 de uma FX30, FX3, série a7 ou a6000, ignora-os.

O Log MetaRaw lê esses dados e usa-os. É um script e cinco nós, numerados nos ícones pela ordem da cadeia:

| | Onde | O que faz |
|---|---|---|
| **Script** | Workspace › Scripts › Log MetaRaw | lê os metadados de todos os clipes do projeto e grava-os no Media Pool |
| Nó **1 · Log MetaRaw Noise** | Color › OpenFX | redução de ruído na imagem log, com o ruído esperado por câmera, ISO e formato; temporal e espacial, GPU Metal |
| **2 · Stabilize**, **3 · Motion Blur** | Color › OpenFX › Log MetaRaw | estabilização por giroscópio (Gyroflow integrado) e desfoque de movimento a partir do fluxo óptico; só Mac Apple silicon |
| Nó **4 · Log MetaRaw** | Color › OpenFX | revela um clipe a partir dos valores de gravação: balanço, exposição, espaço de cor, tons por zonas, falsa cor |
| Nó **5 · Log MetaRaw Creative** | Color › OpenFX | revelação criativa: 16 processos fotográficos (negativos e cópias, reversíveis, preto e branco, Technicolor, Autochrome, três chapas, daguerreótipo, colódio, cópias antigas), vídeo analógico e câmeras digitais de época; cópias e gerações de cópia; presets por dispositivo e modelo (câmeras de cinema, videocâmeras de tubo e CCD, câmeras digitais e telefones), 14 lentes de época, cópias (telecine, cinescópio, film-out) e um suporte de visualização (sala, projetores, TVs CRT e Trinitron, plasma, primeiros LCD, console, papel) |
| Plugins **Log MetaRaw Ripresa** e **Mix e supporto** (VST3) | Fairlight › Efeitos › VST3 | som de época: gravadores e microfones reais nas faixas, mesa, suporte (fita, cassete, vinil, VHS, banda ótica, rádio, telefone), cópias e escuta em buses e master; cada etapa pode ser desligada; Mac e Windows |

Nenhum arquivo é transcodificado e nenhum original é alterado.

### O que esperar, com honestidade

**Não é raw.** Um MP4 log já está demosaicado e comprimido, em 8 ou 10 bits, muitas vezes 4:2:0, com a redução de ruído
da câmera já aplicada. Nenhum plugin consegue devolver o que a câmera descartou.

O que o nó faz é aplicar a ciência de cor com rigor. Exposição e balanço trabalham em luz linear, partem dos valores que
a câmera registrou e seguem curvas e gamuts publicados. Os tons movem a imagem em stops. Trabalhada assim, uma imagem log
**comporta-se de um modo que lembra um arquivo RAW**: o balanço desloca-se limpo, a exposição move-se como um stop de
luz e as altas luzes arredondam em vez de quebrar.

Num trabalho intenso, porém, a ciência de cor sozinha não chega. Assim que os limites da câmera são forçados, a falta de
informação na imagem aparece: banding nos céus, ruído nas sombras levantadas, altas luzes estouradas que continuam
estouradas, cor que se desfaz nos canais comprimidos. Exponha bem na gravação. O Log MetaRaw ajuda a tirar o melhor do
que existe. Não pode criar o que não existe.

---

## Instalação

1. Baixe `SLogMetaRaw-4.3.1.dmg` em **Releases** e abra-o.
2. Dê um duplo clique em **Installa Log MetaRaw.pkg**. Não está assinado com um certificado da Apple: na primeira vez,
   clique com o botão direito › **Abrir**. Pede a senha do Mac porque o plugin vai para uma pasta do sistema.
3. Reinicie o DaVinci Resolve.

O instalador também apaga o cache de plugins do Resolve (`OFXPluginCacheV2.xml`), que o Resolve reconstrói na próxima
inicialização. Sem isso, o Resolve continuaria mostrando o painel antigo e não veria o nó Creative.

| Instalado | Caminho |
|---|---|
| Os dois nós (um único bundle) | `/Library/OFX/Plugins/SLogMetaRaw.ofx.bundle` |
| A biblioteca Python | `/Library/Application Support/SLogMetaRaw/lib/slogmetaraw` |
| O script do menu | `…/DaVinci Resolve/Fusion/Scripts/Utility/Log MetaRaw.py` |

Enquanto funciona, guarda um pequeno registro JSON por clipe em `~/Library/Application Support/SLogMetaRaw/cache`. O
disco também traz os guias ilustrados de onze páginas, em italiano e inglês.

**Instalação limpa e desinstalação.** Cada instalação começa limpa: o instalador substitui por inteiro o plugin e a
biblioteca anteriores e remove as instalações de desenvolvimento, então não fica nenhum arquivo de uma versão antiga.
**Disinstalla Log MetaRaw.command**, no disco (na primeira vez: botão direito › Abrir), remove todas as versões
instaladas, 1.x e 2.x incluídas, com o cache, as configurações e os logs. Antes de mexer em algo lista tudo, pede para
fechar o Resolve e pergunta se deve apagar também os CSV exportados. Os metadados já gravados nos projetos do Resolve
fazem parte dos projetos e ficam.

**Requisitos:** macOS 12 ou posterior, Apple silicon ou Intel, e DaVinci Resolve 21 ou 20. O Resolve 20 não traz
Python próprio: instale o Python 3 de [python.org](https://www.python.org/downloads/), que o menu Workspace › Scripts
precisa de qualquer forma. **Testado apenas no Resolve Studio 21.1 em macOS; Resolve 20 ainda não testado.**

**Clipes:** Sony XAVC em `.MP4` ou `.MXF`, mais as câmeras de [docs/CAMERAS.md](docs/CAMERAS.md) (Panasonic V-Log,
Apple Log, DJI D-Log e D-Log M e outras), cada uma com a confiabilidade da sua leitura. O script lê todos. Os nós revelam
**S-Log3** (S-Gamut3.Cine ou S-Gamut3), **S-Log2**, **S-Log** (S-Gamut) e as curvas dessas câmeras. Desde a 3.2.0 o DJI
D-Log M usa a curva medida numa Osmo Action 6 ([docs/DJI_LOG.md](docs/DJI_LOG.md)): para as outras câmeras DJI é a
melhor estimativa, não uma calibração. Com um perfil desconhecido ficam neutros e avisam.

---

## Em resumo

1. Importe o material. **Workspace › Scripts › Log MetaRaw**: clique **1 · Ler metadados** e depois
   **2 · Gravar no Resolve**.
2. Na página Color, coloque o **Log MetaRaw** como **primeiro nó**. Ele pega o EI, os Kelvin e o tint do clipe. Nesses
   valores não muda nada.
3. Corrija balanço e exposição no nó. As vistas de falsa cor ajudam.
4. Molde os tons com **Toni** (Tons). Para o visual de um filme, um processo antigo, uma fita ou uma câmera de época,
   acrescente o **Log MetaRaw Creative** como nó seguinte.
5. Depois, o seu CST, LUT ou DRT.

```
Noise  →  Stabilize  →  Motion Blur  →  Log MetaRaw  →  Log MetaRaw Creative  →  CST / LUT / DRT  →  resto da correção
```

---

## O script

A janela tem uma linha de botões, uma de opções e a lista de clipes, e segue o idioma do Resolve (português, inglês,
italiano, espanhol, chinês simplificado). Os painéis dos nós estão em italiano: abaixo, os rótulos vêm com tradução.

- Um menu escolhe os clipes: *Todo o Media Pool* ou *Clipes selecionados no Media Pool*.
- **1 · Ler metadados**: uma linha por clipe com câmera, objetiva, abertura, obturador, EI, WB, espaço de cor e
  data level. A coluna *Estado* diz `lido`, o que mudou durante a tomada (abertura, foco…) ou por que um
  clipe foi pulado. Clique numa linha para ver tudo o que foi lido, agrupado como no Catalyst Browse.
- **2 · Gravar no Resolve**: preenche os campos do Media Pool (painel Metadata, colunas, palavras-chave para smart
  bins, data burn-in) e corrige valores que o Resolve lê errado dos MXF, como *Camera Aperture* `F53343` na FX6.
- **Exportar CSV**: exporta os valores para os quais o Resolve não tem campo (EI, tint, modo WB, distância de foco, gama
  de captura…) no formato CSV de metadados do próprio Resolve.
- Opções:
  - **Tag por câmera** (desligada): acrescenta câmera, gama e primárias às palavras-chave.
  - **Sobrescrever metadados** (ligada): substitui os valores que o Resolve já gravou; desligada, preenche só os campos
    vazios.
  - **Corrigir o Data Level** (ligada): coloca o *Data Level* de cada clipe em Full ou Video, conforme a gama exige. O
    porquê está em [docs/DATA_LEVELS.md](docs/DATA_LEVELS.md).
  - **Definir também Input Color Space** (desligada): para projetos com gerenciamento de cor.
    Um script não consegue voltá-lo para *Project*: só você, à mão.
- A versão no canto inferior direito, clicada, consulta o GitHub.

A leitura é rápida porque nada é decodificado: no máximo 24 amostras da faixa de metadados por clipe, com limite de um
segundo. Um volume que para de responder é pulado uma única vez, com um aviso, em vez de travar a lista.

---

## O nó Log MetaRaw

O nó é pontual: cada pixel depende só de si mesmo. Nunca cria halos e o **Generate LUT** pode exportá-lo
(recomendados 65 pontos).

| Controle | O que faz |
|---|---|
| **Versão** (no topo) | `v4.3.1`. Uma vez por dia pergunta ao GitHub pela última release. Se houver, mostra **🟢 v3.1.0 → 3.x.y** e o clique abre no navegador o download do instalador do seu sistema (`.dmg`, `.exe`, `.deb`/`.rpm`/`.run`). Nunca instala nada sozinho |
| **Camera** · **Rileggi metadata** | a câmera lida. *Rileggi* (reler) lê o clipe de novo, devolve cada controle aos valores da câmera e grava os metadados do clipe no Media Pool |
| **Ripresa** (gravação) · **Decodifica** | *Clip* permite mudar os controles; *Camera metadata* trava-os nos valores de gravação |
| **Bilanciamento** (balanço) · **Temperatura** · **Tint** | As shot ou um preset. Adaptação cromática Bradford em luz linear, a partir do branco registrado pela câmera |
| **Punto bianco** (ponto branco) | o conta-gotas do Resolve: escolha na imagem um ponto que deve ser neutro e Temperatura e Tint se ajustam sozinhos, com a mesma matemática do render |
| **Esposizione** (EI) | índice de exposição: o dobro do EI = +1 stop |
| **False color** | uma caixa acima de Temperatura, uma abaixo de Tint e uma abaixo de Esposizione; veja abaixo |
| **Spazio colore** (espaço de cor) | três subgrupos. **Ingresso** (entrada): Spazio colore e Gamma que entram no nó, Rilevato (detectado) e *Ripristina da camera*, Data level, Livello e *Ripristina da clip*. **Senza metadata**: *Sblocca controlli* e as referências de gravação. **Uscita** (saída): **Tone mapping** (Nessuno/CST, ACES 2.0, Luminanza, Filmico, ACES 1.3, OpenDRT), Compressione e Mappatura gamut, Vibrance luci, Spazio colore e Gamma (como um Color Space Transform; *Timeline* não converte), tamanho e exportação da LUT |
| **Toni** (tons) | Highlights, **Bianco** (stop), Soft Clip, Saturation (−100…+100). *Mostra tutti i controlli* acrescenta Contrast, Shadows, Whites, Blacks, Vibrance e as **Zone** (Black, Shadow, Light, Specular com Exp, Sat, Range e Falloff, Contrast Pivot, false color *Zone*); aparecem sozinhos se o nó os usa |
| **Dati di ripresa** (dados de gravação) | só leitura: objetiva, focal, abertura, foco, obturador, EI, WB, fps, ND, LUT da câmera |

**Highlights é um ombro de película.** Em negativo comprime as altas luzes com uma inclinação que diminui de forma
contínua para cima, como a película, o ACES 2.0 e o AgX. A −100 o valor mais alto que a câmera registrou (cerca de +6 stops
acima do cinza em S-Log3) chega exatamente ao **Bianco**: sem véu cinza e sem clip. O cinza e o que está abaixo não se
movem, e a pele a +1 stop se desloca no máximo 0,05 stop. As luzes mais comprimidas vão suavemente para o branco sem mudar
de matiz. Em positivo dá mais força às altas luzes, com inclinação limitada. **Bianco** é onde esse máximo chega (e o teto do
Soft Clip): 2,5 stops é o branco Rec.709 com um CST sem tone mapping. Com um DRT depois do nó (ACES, AgX, DaVinci), suba
para 4–5, senão as altas luzes são comprimidas duas vezes.

**Tons por zonas.** Shadows, Whites e as Zonas são exposições, em stops, sobre uma faixa de tons: Shadows abaixo de −1 stop,
Whites a partir de +3,5 stops. Blacks é um véu linear que move o preto sem mover o cinza. Dentro de uma zona a imagem se
move como numa exposição, então a textura se mantém, e a compressão fica numa faixa de transição declarada. Nenhuma
combinação de controles pode solarizar. O custo de um nó pontual, dito claramente: o que ele comprime, comprime também na
textura. Por isso a recuperação local é um nó separado. Fórmulas e receitas: [docs/TONE_MAPPING.md](docs/TONE_MAPPING.md).

**Falsa cor.** Uma vista por controle, ao lado do controle deslizante a que serve (acima de Temperatura, abaixo de Tint e de Esposizione):
- **Exposição** usa faixas em stops em torno do cinza 18%, no estilo ARRI. Verde é o cinza médio, rosa um stop acima (a
  pele), amarelo perto do clip, vermelho o clip, azul e violeta o fundo.
- **Temperatura** e **Tint** funcionam como no CineMatch. A imagem fica cinza, as dominantes ganham laranja/azul ou
  verde/magenta, e as quase neutras são amplificadas até 8× para aparecerem. Mova o controle até o que deve ser neutro
  ficar cinza. Cada vista responde só ao seu controle.

A vista substitui a imagem: desligue-a antes de renderizar. [docs/FALSE_COLOR.md](docs/FALSE_COLOR.md)

**Data level.** Se o Resolve decodifica um clipe na escala de code values errada, o nó corrige antes da curva log. A
opção *Corrigir o Data Level* do script resolve isso para o projeto inteiro. [docs/DATA_LEVELS.md](docs/DATA_LEVELS.md)

**Tone mapping com saída de cena (3.2.0).** Com um rendering (ACES 2.0, Filmico, OpenDRT…), um Spazio colore e uma
Gamma de cena como DaVinci WG / DaVinci Intermediate não são mais substituídos por Rec.709 / Gamma 2.4: a imagem
renderizada para Rec.709 viaja nessa codificação, para passá-la a outros plugins (Dehancer…), e um CST para Rec.709 /
Gamma 2.4 sem tone mapping a devolve idêntica. As saídas de tela (Rec.709, P3, Rec.2020) não mudam.

**Sem metadados.** Um ProRes de gravador externo, ou um clipe que não pode ser lido, deixa o nó neutro. Marque
*Spazio colore › Senza metadata › Sblocca controlli* e digite o EI, os Kelvin e o tint de gravação: os controles se ativam e o
nó começa neutro.

**Velocidade.** Abrir um projeto não lê nenhum arquivo. Abrir o painel espera no máximo meio segundo; um disco lento
termina em segundo plano, com limite de 15 segundos. *Rileggi* responde em cerca de 2 segundos no máximo.

---

## O nó Log MetaRaw Creative

Um único motor que refaz **o processo**, não uma imitação do resultado. A luz da cena, decodificada em Rec.2020 linear,
vira até três **registros** com a sua sensibilidade espectral. Cada um é exposto na curva do material e transformado em
imagem como aquele processo fazia: corantes subtrativos, filtros aditivos, um depósito de prata ou platina no papel, um
sinal de vídeo. Cada processo é uma tabela de números e o painel mostra só os controles do processo escolhido.

| Processo | O que modela |
|---|---|
| **Negativo colorido e cópia** | três camadas com contraste, pé e ombro próprios, interimagem, grão por camada, halo, cópia positiva com luzes R G B; 14 filmes, também do Leste (Sovcolor, ORWO, Svema, Gevacolor). *Saída Log* dá o escaneamento do negativo |
| **Reversível**, **Preto e branco** | diapositivo; P&B com sensibilidade azul, orto ou pan e filtro de tomada compensado |
| **Technicolor** três tiras e duas cores | três negativos do prisma, matrizes por embebição, chave K até 1944; só vermelho e verde |
| **Autochrome**, **Dufaycolor** | mosaico aditivo de grãos de fécula tingidos ou retícula regular, visto por transparência |
| **Três chapas**, **Kinemacolor** | exposições em sequência: com *Intervallo* > 0 o nó lê os quadros seguintes e o que se move se separa em cores (Prokudin-Gorski) |
| **Daguerreótipo**, **Colódio**, papel salgado e albumina, cianotipia, platina | depósitos sensíveis ao azul e ao UV, espelho e oxidação, suporte, borda, viragem |
| **Vídeo analógico** | 11 formatos, do broadcast ao VHS LP e Betacam SP: banda de luma e cor, atraso da cor, peaking, ruído, troca de cabeças, dropouts |
| **Digital de época** | 10 câmeras: clip e knee, nitidez eletrônica, ruído, subamostragem, banding |

- Vai **logo depois** do Log MetaRaw. Decodifica em luz linear e sai em Rec.709 · Gamma 2.4 (a cópia projetada), ou na
  codificação de entrada com *Saída Log*.
- É **espacial**: o Generate LUT o exclui. Os raios seguem a altura do quadro. Ruído determinístico: CPU e Metal dão a
  mesma imagem.
- Substitui o nó Detail no mesmo lugar: uma correção salva com o Detail abre como um Creative novo.

Os números são estimativas da pesquisa em [docs/CREATIVE.md](docs/CREATIVE.md), não perfis de produtos: precisam ser
ajustados com material real. Ainda não testado no Resolve; Metal verificado pelos testes da CI do macOS.

**Novidades da 4.3.1.**

- **Áudio, escolhas coerentes**: nos dois plugins de áudio cada menu oferece só o que combina com o escolhido
  acima: primeiro o equipamento do gravador, depois as alternativas compatíveis (nada de VHS num Nagra); em *Mix e
  supporto* a mesa decide os suportes do máster e o máster para onde se pode transferir.
- **Pelo na janela, poeira e riscos reais**: o pelo afina para a ponta, tem densidade irregular e uma sombra
  desfocada como no filme (mais no Super 8 e nas cópias); às vezes é encaracolado ou uma fibra de dois fios. A poeira
  tem bordas irregulares e grãos de todo tamanho; os riscos mudam de espessura e intensidade.
- **Digitalização com as marcas do fabricante**: de borda a borda a margem traz a marca (EASTMAN antes de 1990,
  depois KODAK; FUJI, AGFA, ORWO…), o código do filme naquele formato, NITRATE FILM, SAFETY FILM ou ESTAR, um número
  progressivo e desde 1990 o KeyKode com código de barras; filmes de foto numeram os fotogramas. A luz do scanner
  brilha em volta das perfurações e no corte da base. Novo controle *Overscan*.
- **Trilha sonora nas cópias**: de borda a borda, uma cópia de 35 mm ou 16 mm mostra a trilha sonora nas posições
  SMPTE: pista óptica (densidade variável, área variável, Dolby Stereo), desde 1993 Dolby Digital entre as
  perfurações, DTS e SDDS, desde 2004 a pista ciano; Super 8 e 16 mm podem ter pista magnética. Novo menu *Colonna
  sonora della copia* (pela época ou à mão).

**Novidades da 4.3.0.**

- **Películas por formato**: com uma câmera escolhida aparecem só as películas feitas no seu formato, de todos os
  fabricantes e com o código desse formato (em 16 mm a 500T é 7219); as listas completas ficam a uma caixa.
- **Digitalização**: o quadro, o quadro com as bordas da janela ou a película de borda a borda com perfurações, linhas
  entre quadros e quadros vizinhos, com as medidas reais de cada formato. Janela quadrada, arredondada ou limada aberta.
- **O negativo como é**: o negativo colorido contra a luz, complementar sobre a máscara laranja, com o seu grão.
- **Suporte da película**: nitrato, acetato ou poliéster (ou por época e formato) e a sua degradação: manchas âmbar do
  nitrato, cristais e instabilidade do acetato.
- **Oscilação realista** na filmagem, na cópia e na projeção, cada uma com a sua caixa; **pelo na janela** conforme a
  câmera.
- **Consoles** como meio de visualização: NES, Master System, Mega Drive, Super Nintendo, PlayStation e Saturn numa TV
  de tubo.
- Corrigido o fecho ao escolher os meios de visualização adicionados na 4.2.0.

**Novidades da 4.2.0.**

- **Dispositivo › modelo**: parte-se da máquina (câmera de cinema, videocâmera, cinema digital, telefone), que carrega
  formato, fita, sensor e defeitos; depois fabricante e filme, com nomes reais. Os interruptores *Tom*, *Cor* e
  *Textura* permitem usar, por exemplo, só o grão no seu próprio vídeo.
- **Vídeo emulado**: fita VHS e Betacam refeita linha a linha (ruído FM, cor atrasada, cabeças, dropouts) e sensor da
  câmera: tubos com rastro e caudas de cometa, smear do CCD, falsa cor dos tubos de listras.
- **Digitais por modelo**: tamanho do sensor e aliasing, nitidez interna, blocos 8x8 do codec e cor subamostrada (do
  Motorola RAZR à ARRI Alexa).
- **Copiado para**: a cópia em VHS, Betacam, VHS de terceira geração, cinescópio 16 mm ou film-out 35 mm, grão sobre
  grão.
- **Lente**: 14 objetivas (Cooke, Zeiss Super Speed, Canon K35, Panavision, LOMO, Helios, Petzval…) com distorção,
  bordas suaves, redemoinho, véu e faixa anamórfica.
- **Novos meios de exibição**: projetor Super 8, Trinitron, plasma, primeiro LCD.

---

## Os nós Noise, Stabilize e Motion Blur (4.0.0)

Trabalham sobre a imagem da câmera, antes do **Log MetaRaw**.

- **Log MetaRaw Noise** sabe quanto ruído esperar e onde. Do registro do script lê a câmera, o ISO (ou o EI e o ISO
  nativo), o codec, os bits por pixel e o 4:2:0. Tem perfis para Sony FX6 e FX30, iPhone 17 Pro e DJI Osmo Action 4
  e 5 Pro. O botão **Rileggi metadata** relê-os do ficheiro (são lidos sozinhos ao abrir o painel se faltarem). Para o afinar no ruído real marca **Mostra campione**, leva os dois cantos do retângulo a uma zona plana e prime **Usa il campione** (como o Neat Video); bandas, sombras e pele estão no grupo **Avanzate**.
  - *Temporal*: de ±1 a ±3 quadros vizinhos, alinhados por blocos e ponderados pixel a pixel, para que as partes em
    movimento e os cortes não borrem.
  - *Espacial*: luminância e cor separadas, em 5 bandas wavelet. Cada coeficiente é comparado com a energia da sua
    vizinhança, o que mantém a textura.
  - Vistas do ruído removido, proteção de detalhe e pele, e **Salta Noise ovunque** para desligar todos os nós Noise
    enquanto edita.
  - No Mac tudo roda na GPU Metal, e os testes verificam que dá o mesmo resultado da CPU. Nos outros sistemas roda na
    CPU.
  - Os perfis são estimativas até serem calibrados com amostras reais. [docs/DENOISE.md](docs/DENOISE.md)
- **Stabilize** lê o giroscópio do clipe (Gyroflow integrado). Tem estilos do micromovimento à grua e um **MASTER**
  animável. **Motion Blur** reconstrói o obturador a partir do fluxo óptico, por exemplo de 50p a 90° para 25p a
  180°, e mantém o grão do sensor nas zonas com movimento. Ambos vêm do Mecena Motion (`motion/`) e são instalados pelo pacote macOS, só Apple silicon.
- **Aquisição profunda**, uma caixa na janela do script: grava o giroscópio e a objetiva de cada quadro na
  MecenaClipDB, para o Stabilize.

Nenhum dos três nós foi ainda testado no Resolve com clipes reais.

## Log MetaRaw Audio (VST3, 4.2.0)

O gêmeo sonoro do Creative, para o **Fairlight** (Mac e Windows): dois plugins num só módulo, `Log MetaRaw Audio.vst3`.

- **Ripresa** (gravação), nas faixas: gravador (Geloso G.257, Nagra, Uher, Studer, Revox, Walkman, Auricon, fonógrafo
  Edison, gravador de fio…), microfone (o de série ou RCA 44, U 47, C12, SM58, MKH 416…), suporte e redução de ruído.
- **Mix e supporto** (mixagem e suporte), em buses e master: console (RCA, EMI REDD.51, Neve, SSL, Mackie), suporte
  (fita, cassete, 78/45/33 rpm, VHS, trilha ótica, rádio, telefone), cópias e escuta (rádio valvulado, jukebox, TV,
  cinema).
- Cada estágio tem um interruptor; Saturação, Ruído, Wow e flutter, Dry/wet e Saída ajustam o resultado.
- Nos instaladores é um componente à parte, marcado por padrão. Linux: o Resolve não carrega VST3 nativos.

Os valores são estimativas de especificações e descrições de época ([docs/audio/AUDIO.md](docs/audio/AUDIO.md)); ainda
não testado no Fairlight.

## Atualizações e privacidade

- Os **nós** perguntam ao GitHub pela última release deste projeto no máximo uma vez por dia, em segundo plano. A
  requisição leva só a versão do programa (`User-Agent: SLogMetaRaw/4.3.1`). Para desativar, crie o arquivo vazio
  `~/Library/Application Support/SLogMetaRaw/no_update_check`.
- O **script** só consulta quando você clica na sua versão.
- Um clique apenas abre um link de download das releases do GitHub deste projeto. Nada é instalado sem você.

---

## Como funciona, em resumo

- **Metadados.** Os parsers são Python puro, sem dependências. Leem os metadados de aquisição SMPTE RDD 18 quadro a quadro
  (faixa `rtmd` dos MP4, pacotes ST 436 dos MXF), o XML NonRealTimeMeta da Sony, o SPS H.264/HEVC e o descritor de imagem
  MXF. Um clipe de vários gigabytes custa cerca de 100 KB de leitura.
- **Do script ao nó.** Cada clipe tem um registro JSON no cache. O nó encontra o seu arquivo pelo caminho de origem que o
  Resolve lhe passa, lê o registro e, se faltar, cria-o em segundo plano.
- **Matemática da cor.** A cadeia é: decodificação da curva da câmera, razão de EI, balanço Bradford a partir do lócus de
  Planck (com o tint perpendicular em CIE 1960 uv), depois tons com um único ganho por pixel a partir de uma norma dos
  seus canais (a cromaticidade se mantém), depois cor, depois saída. Tudo em luz linear, com as curvas e gamuts
  publicados pela Sony.
- **Uma matemática, três implementações.** A matemática fica em `ofx/SLogMetaRaw/math` e é compilada como C++ e como
  Metal. Um modelo Python de referência verifica as duas: CPU e GPU concordam dentro de 2·10⁻⁴, com FMA desativado em
  todos os caminhos.
- **Testes.** 520 testes automáticos: parsers, modelos de referência, C++, Metal, um host OpenFX em miniatura que carrega
  os dois nós como o Resolve faz, e arquivos golden dos painéis.

---

## Compilar a partir do código

```bash
make -C ofx/SLogMetaRaw                     # plugin universal, arm64 + x86_64 (requer Xcode)
make -C ofx/SLogMetaRaw test-bins           # binários de teste
python3 -m unittest discover tests          # os testes
./install.sh --dev                          # script e plugin apontando para esta pasta
./packaging/build_installer.sh              # dist/SLogMetaRaw-<versão>.dmg
python3 -m slogmetaraw clip.MP4             # leitura estilo Catalyst pela linha de comando
```

```
slogmetaraw/         parsers, gravação no Resolve, janela do script, verificação de atualizações
resolve_script/      lançador para Workspace › Scripts
ofx/SLogMetaRaw/     math/ (compartilhada CPU/Metal), src/ (common, develop, detail, noise), metal/
motion/              Mecena Motion: Stabilize, Motion Blur, mecena-clipdb (Rust + C++, macOS)
tools/               geradores de matrizes, ícones e gráficos
tests/               testes e modelos de referência (os clipes de teste não estão no repositório)
packaging/           instalador, guias, desinstalador
docs/                TONE_MAPPING, CREATIVE, DENOISE, FALSE_COLOR, DATA_LEVELS
```

### Linux e Windows (renderização por CPU, experimental)

O plugin também compila para Linux e Windows com CMake. Esses ports renderizam na CPU (ainda sem Metal, CUDA ou
OpenCL) e não foram testados dentro do DaVinci Resolve: a build compatível continua sendo a do macOS.

**Instalação no Linux** (baixe em **Releases** o arquivo certo para sua distribuição):

| Arquivo | Para |
|---|---|
| `slogmetaraw_<versão>_amd64.deb` | Ubuntu, Debian, Mint: `sudo apt install ./slogmetaraw_*.deb` |
| `slogmetaraw-<versão>-1.x86_64.rpm` | Rocky, Alma, RHEL, CentOS, Fedora: `sudo dnf install ./slogmetaraw-*.rpm` |
| `SLogMetaRaw-<versão>-linux-x86_64.run` | qualquer distribuição: `sh SLogMetaRaw-*.run` (`--user` sem sudo, `--uninstall`) |
| `SLogMetaRaw-<versão>-linux-x86_64.tar.gz` | o mesmo, descompactado: execute `install.sh` |

O plugin vai para `/usr/OFX/Plugins`; a biblioteca Python para `/usr/lib/slogmetaraw` (.deb/.rpm) ou
`~/.local/share/SLogMetaRaw` (.run, .tar.gz). O pacote coloca o script de menu em
`/opt/resolve/Fusion/Scripts/Utility` quando o Resolve está lá; caso contrário, execute uma vez `slogmetaraw-setup`
(para o seu usuário). Reinicie o Resolve. Basta o Python 3.6 (mesmo no Rocky/RHEL 8). O binário é compilado contra
glibc 2.28: carrega no Rocky/Alma/RHEL 8+, Ubuntu 20.04+ e Debian 10+.

```bash
# compile você mesmo (cmake, um compilador C++17, python3; o SDK OpenFX é baixado, ou defina OFX_SDK_DIR)
./packaging/linux/build_package.sh          # dist/: .tar.gz, .run, .deb, .rpm
# Windows (PowerShell): cmake, Visual Studio Build Tools, python; Inno Setup é opcional
powershell -File packaging\windows\build_package.ps1   # dist\...-windows-x64.zip (+ instalador .exe com Inno Setup)
```

O estado fica em `~/.local/share/SLogMetaRaw` no Linux e `%APPDATA%\SLogMetaRaw` no Windows. `.github/workflows/`
compila os pacotes e instala o .deb/.rpm no Rocky 8/9, Ubuntu 20.04-24.04 e Debian 12 a cada mudança; enviar uma tag
`v*` os anexa a uma release em rascunho.

---

## Limites conhecidos

- O painel Camera Raw do Resolve e a sua estabilização por giroscópio não podem ser desbloqueados para MP4 (para o giroscópio há o nó Stabilize): vivem dentro dos
  decodificadores do Resolve. O Log MetaRaw reconstrói os controles de cor; não abre uma porta dentro do Resolve.
- Algumas câmeras (entre elas a a6300) não registram os Kelvin: nesse caso são estimados a partir do preset de luz e
  sinalizados.
- O S-Log2 segue o documento da Sony. A curva S-Log2 do Resolve difere em cerca de 0,15 stop.
- Ainda por verificar em mais arquivos: XAVC HS (HEVC), HLG, S-Cinetone, zooms motorizados.
- **Um projeto independente e amador**, distribuído como está, sem garantia e sem responsabilidade pelo uso profissional.
  O código é aberto: leia, teste, altere.

---

## Créditos e licença

**Ivan Mazzone + Claude** · [github.com/ivan-94m](https://github.com/ivan-94m) · [@ivan_94m](https://instagram.com/ivan_94m).
Escrito com o Claude (Anthropic): a 1.0 em 19 de setembro de 2026, a 1.1.0 em 22 de setembro, a 2.0.0 em 23 de setembro
de 2026, a 3.2.0 e a 4.0.0 em 24 de novembro de 2026. A história completa está em [RELEASE_NOTES.md](RELEASE_NOTES.md) (em italiano).

Referências para as tags Sony: SMPTE RDD 18, [ExifTool](https://exiftool.org) e
[telemetry-parser](https://github.com/AdrianEddy/telemetry-parser) de AdrianEddy (MIT), de onde vem parte da tabela de
tags.

Rendering e dados de câmera: o Output Transform ACES 2.0 é portado do OpenColorIO 2.6 e o ACES 1.3 é verificado
contra as suas transformações embutidas (ACES: Academy of Motion Picture Arts and Sciences); o [OpenDRT](https://github.com/jedypod/open-display-transform)
v1.1.0 é de Jed Smith (GPL-3.0), portado do seu DCTL. A curva e o gamut DJI D-Log M vêm das medições da Osmo Action 6
em [jens-krause/dlogm-dctl](https://github.com/jens-krause/dlogm-dctl) v1.3 e da LUT DJI da Action 6, usada só como referência e
não distribuída; até a 3.1.x a curva era o ajuste de Thatcher Freeman.

O Stabilize baseia-se no [Gyroflow](https://github.com/gyroflow/gyroflow) de AdrianEddy e colaboradores (GPL-3.0) e no seu
catálogo de objetivas. O filtro espacial do nó Noise segue o NeighShrink (Chen, Bui e Krzyżak, 2004).

[GNU GPL v3.0 ou posterior](LICENSE). O SDK OpenFX é © The Open Effects Association (BSD-3). O SDK VST3 é © Steinberg Media Technologies (MIT); VST é uma marca da Steinberg. Sony, XAVC e Catalyst são
marcas da Sony Group Corporation; DaVinci Resolve é uma marca da Blackmagic Design. Projeto independente, não afiliado nem
endossado por nenhuma das duas empresas.
