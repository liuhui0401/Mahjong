

#include <iostream>
#include <set>
#include <string>
#include <cassert>
#include <cstring> // 注意memset是cstring里的
#include <algorithm>
#include <ctime>
#include <cstdlib>
#include <exception>
#include "jsoncpp/json.h" // 在平台上，C++编译时默认包含此库

using std::cin;
using std::cout;
using std::endl;
using std::vector;
using std::sort;
using std::unique;
using std::set;
using std::string;
using std::time;
using std::srand;
using std::rand;
using std::exception;
constexpr int PLAYER_COUNT = 3;

enum class Stage
{
    BIDDING, // 叫分阶段
    PLAYING     // 打牌阶段
};
 
enum class CardComboType
{
    PASS, // 过
    SINGLE, // 单张
    PAIR, // 对子
    STRAIGHT, // 顺子
    STRAIGHT2, // 双顺
    TRIPLET, // 三条
    TRIPLET1, // 三带一
    TRIPLET2, // 三带二
    BOMB, // 炸弹
    QUADRUPLE2, // 四带二（只）
    QUADRUPLE4, // 四带二（对）
    PLANE, // 飞机
    PLANE1, // 飞机带小翼
    PLANE2, // 飞机带大翼
    SSHUTTLE, // 航天飞机
    SSHUTTLE2, // 航天飞机带小翼
    SSHUTTLE4, // 航天飞机带大翼
    ROCKET, // 火箭
    INVALID // 非法牌型
};
 
int cardComboScores[] = {
    0, // 过
    1, // 单张
    2, // 对子
    6, // 顺子
    6, // 双顺
    4, // 三条
    4, // 三带一
    4, // 三带二
    10, // 炸弹
    8, // 四带二（只）
    8, // 四带二（对）
    8, // 飞机
    8, // 飞机带小翼
    8, // 飞机带大翼
    10, // 航天飞机（需要特判：二连为10分，多连为20分）
    10, // 航天飞机带小翼
    10, // 航天飞机带大翼
    16, // 火箭
    0 // 非法牌型
};
 
#ifndef _BOTZONE_ONLINE
string cardComboStrings[] = {
    "PASS",
    "SINGLE",
    "PAIR",
    "STRAIGHT",
    "STRAIGHT2",
    "TRIPLET",
    "TRIPLET1",
    "TRIPLET2",
    "BOMB",
    "QUADRUPLE2",
    "QUADRUPLE4",
    "PLANE",
    "PLANE1",
    "PLANE2",
    "SSHUTTLE",
    "SSHUTTLE2",
    "SSHUTTLE4",
    "ROCKET",
    "INVALID"
};
#endif
 
// 用0~53这54个整数表示唯一的一张牌
using Card = short;
constexpr Card card_joker = 52;
constexpr Card card_JOKER = 53;
 
// 除了用0~53这54个整数表示唯一的牌，
// 这里还用另一种序号表示牌的大小（不管花色），以便比较，称作等级（Level）
// 对应关系如下：
// 3 4 5 6 7 8 9 10    J Q K    A    2    小王    大王
// 0 1 2 3 4 5 6 7    8 9 10    11    12    13    14
using Level = short;
constexpr Level MAX_LEVEL = 15;
constexpr Level MAX_STRAIGHT_LEVEL = 11;
constexpr Level level_joker = 13;
constexpr Level level_JOKER = 14;
 
/**
* 将Card变成Level
*/
constexpr Level card2level(Card card)
{
    return card / 4 + card / 53;
}
 
// 牌的组合，用于计算牌型
struct CardCombo
{
    // 表示同等级的牌有多少张
    // 会按个数从大到小、等级从大到小排序
    struct CardPack
    {
        Level level;
        short count;
 
        bool operator< (const CardPack& b) const
        {
            if (count == b.count)
                return level > b.level;
            return count > b.count;
        }
    };
    vector<Card> cards; // 原始的牌，未排序
    vector<CardPack> packs; // 按数目和大小排序的牌种
    CardComboType comboType; // 算出的牌型
    Level comboLevel = 0; // 算出的大小序
 
                          /*
                          * 检查个数最多的CardPack递减了几个
                          */
    int findMaxSeq() const
    {
        for (unsigned c = 1; c < packs.size(); c++)
            if (packs[c].count != packs[0].count ||
                packs[c].level != packs[c - 1].level - 1)
                return c;
        return packs.size();
    }
 
    /*
     * 这个牌型最后算总分的时候的权重
     */
    int getWeight() const
    {
        if (comboType == CardComboType::SSHUTTLE ||
            comboType == CardComboType::SSHUTTLE2 ||
            comboType == CardComboType::SSHUTTLE4)
            return cardComboScores[(int)comboType] + (findMaxSeq() > 2) * 10;
        return cardComboScores[(int)comboType];
    }
 
    // 创建一个空牌组
    CardCombo() : comboType(CardComboType::PASS) {}
 
    /*
     * 通过Card（即short）类型的迭代器创建一个牌型
     * 并计算出牌型和大小序等
     * 假设输入没有重复数字（即重复的Card）
     */
    template <typename CARD_ITERATOR>
    CardCombo(CARD_ITERATOR begin, CARD_ITERATOR end)
    {
        // 特判：空
        if (begin == end)
        {
            comboType = CardComboType::PASS;
            return;
        }
 
        // 每种牌有多少个
        short counts[MAX_LEVEL + 1] = {};
 
        // 同种牌的张数（有多少个单张、对子、三条、四条）
        short countOfCount[5] = {};
        cards = vector<Card>(begin, end);
//        for (Card c : cards)
//            counts[card2level(c)]++;
        for(int i=0; i<cards.size(); ++i)
        {
            ++counts[card2level(cards[i])];
        }
        for (Level l = 0; l <= MAX_LEVEL; l++)
            if (counts[l])
            {
                packs.push_back(CardPack{ l, counts[l] });
                countOfCount[counts[l]]++;
            }
        sort(packs.begin(), packs.end());
 
        // 用最多的那种牌总是可以比较大小的
        comboLevel = packs[0].level;
 
        // 计算牌型
        // 按照 同种牌的张数 有几种进行分类
        vector<int> kindOfCountOfCount;
        for (int i = 0; i <= 4; i++)
            if (countOfCount[i])
                kindOfCountOfCount.push_back(i);
        sort(kindOfCountOfCount.begin(), kindOfCountOfCount.end());
 
        int curr, lesser;
 
//        switch (kindOfCountOfCount.size())
//        {
//        case 1: // 只有一类牌
        if(kindOfCountOfCount.size() == 1)
        {
            curr = countOfCount[kindOfCountOfCount[0]];
//            switch (kindOfCountOfCount[0])
//            {
//            case 1:
            if(kindOfCountOfCount[0] == 1)
            {
                // 只有若干单张
                if (curr == 1)
                {
                    comboType = CardComboType::SINGLE;
                    return;
                }
                if (curr == 2 && packs[1].level == level_joker)
                {
                    comboType = CardComboType::ROCKET;
                    return;
                }
                if (curr >= 5 && findMaxSeq() == curr &&
                    packs.begin()->level <= MAX_STRAIGHT_LEVEL)
                {
                    comboType = CardComboType::STRAIGHT;
                    return;
                }
//                break;
            }
//            case 2:
            else if(kindOfCountOfCount[0] == 2)
            {
                // 只有若干对子
                if (curr == 1)
                {
                    comboType = CardComboType::PAIR;
                    return;
                }
                if (curr >= 3 && findMaxSeq() == curr &&
                    packs.begin()->level <= MAX_STRAIGHT_LEVEL)
                {
                    comboType = CardComboType::STRAIGHT2;
                    return;
                }
//                break;
            }
//            case 3:
            else if(kindOfCountOfCount[0] == 3)
            {
                // 只有若干三条
                if (curr == 1)
                {
                    comboType = CardComboType::TRIPLET;
                    return;
                }
                if (findMaxSeq() == curr &&
                    packs.begin()->level <= MAX_STRAIGHT_LEVEL)
                {
                    comboType = CardComboType::PLANE;
                    return;
                }
//                break;
            }
//            case 4:
            else if(kindOfCountOfCount[0] == 4)
            {
                // 只有若干四条
                if (curr == 1)
                {
                    comboType = CardComboType::BOMB;
                    return;
                }
                if (findMaxSeq() == curr &&
                    packs.begin()->level <= MAX_STRAIGHT_LEVEL)
                {
                    comboType = CardComboType::SSHUTTLE;
                    return;
                }
            }
//            break;
        }
//        case 2: // 有两类牌
        else if(kindOfCountOfCount.size() == 2)
        {
            curr = countOfCount[kindOfCountOfCount[1]];
            lesser = countOfCount[kindOfCountOfCount[0]];
            if (kindOfCountOfCount[1] == 3)
            {
                // 三条带？
                if (kindOfCountOfCount[0] == 1)
                {
                    // 三带一
                    if (curr == 1 && lesser == 1)
                    {
                        comboType = CardComboType::TRIPLET1;
                        return;
                    }
                    if (findMaxSeq() == curr && lesser == curr &&
                        packs.begin()->level <= MAX_STRAIGHT_LEVEL)
                    {
                        comboType = CardComboType::PLANE1;
                        return;
                    }
                }
                if (kindOfCountOfCount[0] == 2)
                {
                    // 三带二
                    if (curr == 1 && lesser == 1)
                    {
                        comboType = CardComboType::TRIPLET2;
                        return;
                    }
                    if (findMaxSeq() == curr && lesser == curr &&
                        packs.begin()->level <= MAX_STRAIGHT_LEVEL)
                    {
                        comboType = CardComboType::PLANE2;
                        return;
                    }
                }
            }
            else if (kindOfCountOfCount[1] == 4)
            {
                // 四条带？
                if (kindOfCountOfCount[0] == 1)
                {
                    // 四条带两只 * n
                    if (curr == 1 && lesser == 2)
                    {
                        comboType = CardComboType::QUADRUPLE2;
                        return;
                    }
                    if (findMaxSeq() == curr && lesser == curr * 2 &&
                        packs.begin()->level <= MAX_STRAIGHT_LEVEL)
                    {
                        comboType = CardComboType::SSHUTTLE2;
                        return;
                    }
                }
                if (kindOfCountOfCount[0] == 2)
                {
                    // 四条带两对 * n
                    if (curr == 1 && lesser == 2)
                    {
                        comboType = CardComboType::QUADRUPLE4;
                        return;
                    }
                    if (findMaxSeq() == curr && lesser == curr * 2 &&
                        packs.begin()->level <= MAX_STRAIGHT_LEVEL)
                    {
                        comboType = CardComboType::SSHUTTLE4;
                        return;
                    }
                }
            }
        }
 
        comboType = CardComboType::INVALID;
    }
 
    /*
     * 判断指定牌组能否大过当前牌组（这个函数不考虑过牌的情况！）
     */
    bool canBeBeatenBy(const CardCombo& b) const
    {
        if (comboType == CardComboType::INVALID || b.comboType == CardComboType::INVALID)
            return false;
        if (b.comboType == CardComboType::ROCKET)
            return true;
        if (b.comboType == CardComboType::BOMB && comboType == CardComboType::ROCKET)
            return false;
        if (b.comboType == CardComboType::BOMB && comboType == CardComboType::BOMB)
            return comboLevel < b.comboLevel;
        if (b.comboType == CardComboType::BOMB) return true;
//            if(comboType == CardComboType::ROCKET)
//                return false;
//            else if(comboType == CardComboType::BOMB)
//                return comboLevel < b.comboLevel;
//            else return true;
//            switch (comboType)
//            {
//            case CardComboType::ROCKET:
//                return false;
//            case CardComboType::BOMB:
//                return b.comboLevel > comboLevel;
//            default:
//                return true;
//            }
        return b.comboType == comboType && b.cards.size() == cards.size() && b.comboLevel > comboLevel;
    }
 
    /**
    * 从指定手牌中寻找第一个能大过当前牌组的牌组
    * 如果随便出的话只出第一张
    * 如果不存在则返回一个PASS的牌组
    */
    bool judgeSequential() const
    {
        if(comboType == CardComboType::STRAIGHT) return true;
        if(comboType == CardComboType::STRAIGHT2) return true;
        if(comboType == CardComboType::PLANE) return true;
        if(comboType == CardComboType::PLANE1) return true;
        if(comboType == CardComboType::PLANE2) return true;
        if(comboType == CardComboType::SSHUTTLE) return true;
        if(comboType == CardComboType::SSHUTTLE2) return true;
        if(comboType == CardComboType::SSHUTTLE4) return true;
        return false;
    }
    
    template <typename CARD_ITERATOR>
    CardCombo findFirstValid(CARD_ITERATOR begin, CARD_ITERATOR end) const
    {
        if (comboType == CardComboType::PASS) // 如果不需要大过谁，只需要随便出
        {
            CARD_ITERATOR second = begin;
            second++;
            return CardCombo(begin, second); // 那么就出第一张牌……
        }
 
        // 然后先看一下是不是火箭，是的话就过
        if (comboType == CardComboType::ROCKET)
            return CardCombo();
 
        // 现在打算从手牌中凑出同牌型的牌
        auto deck = vector<Card>(begin, end); // 手牌
        short counts[MAX_LEVEL + 1] = {};
 
        unsigned short kindCount = 0;
 
        // 先数一下手牌里每种牌有多少个
//        for (Card c : deck)
//            counts[card2level(c)]++;
        for(int i=0; i<deck.size(); ++i)
        {
            ++counts[card2level(deck[i])];
        }
 
        // 手牌如果不够用，直接不用凑了，看看能不能炸吧
        if (deck.size() < cards.size())
            goto failure;
 
        // 再数一下手牌里有多少种牌
//        for (short c : counts)
//            if (c)
//                kindCount++;
        for(int i=0; i<MAX_LEVEL + 1; ++i)
        {
            if(counts[i]) ++kindCount;
        }
 
        // 否则不断增大当前牌组的主牌，看看能不能找到匹配的牌组
        {
            // 开始增大主牌
            int mainPackCount = findMaxSeq();
            bool isSequential = judgeSequential();
//                comboType == CardComboType::STRAIGHT ||
//                comboType == CardComboType::STRAIGHT2 ||
//                comboType == CardComboType::PLANE ||
//                comboType == CardComboType::PLANE1 ||
//                comboType == CardComboType::PLANE2 ||
//                comboType == CardComboType::SSHUTTLE ||
//                comboType == CardComboType::SSHUTTLE2 ||
//                comboType == CardComboType::SSHUTTLE4;
            for (Level i = 1; ; i++) // 增大多少
            {
                for (int j = 0; j < mainPackCount; j++)
                {
                    int level = packs[j].level + i;
 
                    // 各种连续牌型的主牌不能到2，非连续牌型的主牌不能到小王，单张的主牌不能超过大王
                    if ((comboType == CardComboType::SINGLE && level > MAX_LEVEL) ||
                        (isSequential && level > MAX_STRAIGHT_LEVEL) ||
                        (comboType != CardComboType::SINGLE && !isSequential && level >= level_joker))
                        goto failure;
 
                    // 如果手牌中这种牌不够，就不用继续增了
                    if (counts[level] < packs[j].count)
                        goto next;
                }
 
                {
                    // 找到了合适的主牌，那么从牌呢？
                    // 如果手牌的种类数不够，那从牌的种类数就不够，也不行
//                    if (kindCount < packs.size())
//                        continue;
                    if (kindCount >= packs.size())
                    {
                        // 好终于可以了
                        // 计算每种牌的要求数目吧
                        short requiredCounts[MAX_LEVEL + 1] = {};
                        for (int j = 0; j < mainPackCount; j++)
                            requiredCounts[packs[j].level + i] = packs[j].count;
                        for (unsigned j = mainPackCount; j < packs.size(); j++)
                        {
//                            Level k;
                            bool flag = false;
                            for (Level k = 0; k <= MAX_LEVEL; k++)
                            {
                                if (requiredCounts[k] || counts[k] < packs[j].count)
                                    continue;
                                flag = true;
                                requiredCounts[k] = packs[j].count;
                                break;
                            }
//                            if (k == MAX_LEVEL + 1) // 如果是都不符合要求……就不行了
                            if(!flag)
                                goto next;
                        }
     
     
                        // 开始产生解
                        vector<Card> solve;
//                        for (Card c : deck)
                        for(int i=0; i<deck.size(); ++i)
                        {
                            Level level = card2level(deck[i]);
                            if (requiredCounts[level])
                            {
                                solve.push_back(deck[i]);
                                requiredCounts[level]--;
                            }
                        }
                        return CardCombo(solve.begin(), solve.end());
                    }
                }
 
            next:
                ; // 再增大
            }
        }
 
    failure:
        // 实在找不到啊
        // 最后看一下能不能炸吧
 
        for (Level i = 0; i < level_joker; i++)
            if (counts[i] == 4 && (comboType != CardComboType::BOMB || i > packs[0].level)) // 如果对方是炸弹，能炸的过才行
            {
                // 还真可以啊……
                Card bomb[] = { Card(i * 4), Card(i * 4 + 1), Card(i * 4 + 2), Card(i * 4 + 3) };
                return CardCombo(bomb, bomb + 4);
            }
 
        // 有没有火箭？
        if (counts[level_joker] + counts[level_JOKER] == 2)
        {
            Card rocket[] = { card_joker, card_JOKER };
            return CardCombo(rocket, rocket + 2);
        }
 
        // ……
        return CardCombo();
    }
 
    void debugPrint()
    {
#ifndef _BOTZONE_ONLINE
        std::cout << "【" << cardComboStrings[(int)comboType] <<
            "共" << cards.size() << "张，大小序" << comboLevel << "】";
#endif
    }
};
 
// 我的牌有哪些
set<Card> myCards;
 
// 地主被明示的牌有哪些
set<Card> landlordPublicCards;
 
// 大家从最开始到现在都出过什么
vector<vector<Card>> whatTheyPlayed[PLAYER_COUNT];
 
// 当前要出的牌需要大过谁
CardCombo lastValidCombo;
 
// 大家还剩多少牌
short cardRemaining[PLAYER_COUNT] = { 17, 17, 17 };
 
// 我是几号玩家（0-地主，1-农民甲，2-农民乙）
int myPosition;
 
// 地主位置 设置为0
//int landlordPosition = -1;

// 地主叫分
int landlordBid = -1;

// 阶段
Stage stage = Stage::BIDDING;

// 自己的第一回合收到的叫分决策
vector<int> bidInput;

 
//-----------------------------------------------------------------------------------检测地主的手牌数量？
int last_player_is_friend_ornot = 0, last_player_is_who = 0;//1代表队友，0代表对手，2表示自己
short Curmycard[MAX_LEVEL];
int Cardnum;
double maxscore = 1e10;
int leasttimes = 1e9, max_times = 1e9;
int HowManyTimes = 0;
vector<Card> bestcard;
void evaluate_init()
{
    memset(Curmycard, 0, sizeof(Curmycard));
    for (auto c : myCards)
    {
        Curmycard[card2level(c)]++;
    }
    Cardnum = myCards.size();
    max_times = leasttimes = 1e9;
    maxscore = 1e10;
    HowManyTimes = 0;
    return;
}
set<Card> remain_card; //里面装着剩下的外面还没出的牌
 
//寻找第一个最长的n顺子
//k表示是单顺还是双顺或飞机，s是顺子的长度
int find_straight(int &s, int k, int p, int begin){
    int i, j;
    for (i = begin; i <= 10;){
        while (i <= 10 && Curmycard[i] < k)
            ++i;
        j = 1;
        while (i+j <= 11 && Curmycard[i+j] >= k)
            ++j;
        if (j >= p && i <= 10){
            s = j;
            return i;
        }
        i = i + j;
    }
    s = 0;
    return 0;
}
 
//判断是否全场最大
bool judge_best()
{
    if (CardCombo(bestcard.begin(), bestcard.end()).findFirstValid(remain_card.begin(), remain_card.end()).comboType == CardComboType::PASS)
        return true;
    else
        return false;
}
 
//附加分
double extra_point(const int num, const int i)
{
    if (judge_best())
    {
        int n = 0;
        for (int i = 0; i < MAX_LEVEL; ++i)
            if (Curmycard[i] > 0)
                ++n;
        if (n <= 1)
            return -1e9;
    }
    double extra = 0;
    if (myPosition == 0)
    {
        if (cardRemaining[1] < 3 && num == cardRemaining[1])
            extra += (8 - i/2) / 10.0;
        if (cardRemaining[2] < 3 && num == cardRemaining[2])
            extra += (8 - i/2) / 10.0;
    }
    else
    {
        if (cardRemaining[0] < 3 && num == cardRemaining[0] && last_player_is_friend_ornot == 2)
            extra += (8 - i/2) / 10.0;
    }
    return extra;
}
 
#define usefulcard 11
#define scale1 0.1
#define score1 1 //普通炸弹
#define score2 1.2 //王炸
#define score3 1 //飞机
#define score4 0.5 //飞机
#define score5 0.2 //飞机
#define score6 -0.5 //顺子
#define score7 -0.1 //四带二单张
#define score8 0 //四带二双张
#define score9 -1 //对子或单张
#define score10 0 //对子或单张
#define score11 0.5 //对子或单张
#define score12 1 //对子或单张

//评估函数,用来评价当前的手牌质量，分数越低手牌质量越好
double evaluate(int depth, int kth, double &score)
{
    if (Cardnum <= 0 || kth >= 6)
    {
        score += scale1 * HowManyTimes;
        if (maxscore > score)
        {
            maxscore = score;
            max_times = HowManyTimes;
        }
        if (HowManyTimes < leasttimes)
            leasttimes = HowManyTimes;
        score -= scale1 * HowManyTimes;
        return 0;
    }
    switch (kth)
    {
    case 0:
        //找炸弹
        for (int i = 0; i < MAX_LEVEL - 2; ++i)
        {
            if (Curmycard[i] == 4)
            {
                score -= score1;
                //炸弹想出就出，分数最低
                Curmycard[i] = 0;
                Cardnum -= 4;
                HowManyTimes++;
                evaluate(depth + 1, 0, score);
                HowManyTimes--;
                Curmycard[i] = 4;
                Cardnum += 4;
                score += score1;
            }
        }
        if (Curmycard[MAX_LEVEL - 2] == 1 && Curmycard[MAX_LEVEL - 1] == 1)//王炸
        {
            score -= score2;
            Curmycard[MAX_LEVEL - 2] = 0;
            Curmycard[MAX_LEVEL - 1] = 0;
            Cardnum -= 2;
            HowManyTimes++;
            evaluate(depth + 1, 1, score);
            HowManyTimes--;
            Curmycard[MAX_LEVEL - 2] = 1;
            Curmycard[MAX_LEVEL - 1] = 1;
            Cardnum += 2;
            score += score2;
        }
 
    case 1:
        //找顺子（k顺）
    {
        int s = 0;
        int i = find_straight(s, 1, 5, 0);
        if (s >= 5)
        {
            for (int b = i; b <= i + s - 5; ++b)
            {
                for (int k = 5; k <= s && b + k <= i + s; ++k)
                {
                    for (int j = 0; j < k; ++j)
                    {
                        Curmycard[b + j]--;
                    }
                    Cardnum -= k;
                    score -= score6;
                    HowManyTimes++;
                    evaluate(depth + 1, 1, score);
                    HowManyTimes--;
                    for (int j = 0; j < k; ++j)
                    {
                        Curmycard[b + j]++;
                    }
                    Cardnum += k;
                    score += score6;
                }
            }
 
        }
    }
 
 
    case 2:
        //找对顺
    {
        int s = 0;
        int i = find_straight(s, 2, 3, 0);
        if (s != 0)
        {
            for (int b = i; b <= i + s - 3; ++b)
            {
                for (int k = 3; k <= s && b + k <= i + s; ++k)
                {
                    for (int j = 0; j < k; ++j)
                    {
                        Curmycard[b + j] -= 2;
                    }
                    Cardnum -= k * 2;
                    score -= score6;
                    HowManyTimes++;
                    evaluate(depth + 1, 2, score);
                    HowManyTimes--;
                    for (int j = 0; j < k; ++j)
                    {
                        Curmycard[b + j] += 2;
                    }
                    Cardnum += k * 2;
                    score += score6;
                }
            }
        }
    }
 
    case 3:
        //飞机或三个头
    {
        int s = 0;
        int i = find_straight(s, 3, 1, 0);
 
 
        int wings[6], pair = 0;
        int wing[6], single = 0;
        //有三个头
        if (s != 0)
        {
            for (int j = 0; j < s; ++j)
            {
                Curmycard[i + j] -= 3;
            }
            Cardnum -= 3 * s;
            for (int t = 0; t < 12 && (t < i || t >= i + s); ++t)
            {
                if (Curmycard[t] == 1 && single < s)
                {
                    wing[single] = t;
                    single++;
                }
                else if (Curmycard[t] == 2 && pair < s)
                {
                    wings[pair] = t;
                    pair++;
                }
            }
            //飞机带小翼
            if (single == s)
            {
                for (int o = 0; o < s; ++o)
                {
                    Curmycard[wing[o]] = 0;
                }
                Cardnum -= s;
                if (i <= 7)
                {
                    score -= score3;
                }
                else if(s == 1)
                {
                    score -= score4;
                }
                else
                {
                    score -= score5;
                }
                HowManyTimes++;
                evaluate(depth + 1, 3, score);
                HowManyTimes--;
                for (int o = 0; o < s; ++o)
                {
                    Curmycard[wing[o]] = 1;
                }
                Cardnum += s;
                if (i <= 7)
                {
                    score += score3;
                }
                else if (s == 1)
                {
                    score += score4;
                }
                else
                {
                    score += score5;
                }
            }
 
            //飞机带大翼
            if (pair == s)
            {
                for (int o = 0; o < s; ++o)
                {
                    Curmycard[wings[o]] = 0;
                }
                Cardnum -= s * 2;
                if (i <= 7)
                {
                    score -= score3;
                }
                else if (s == 1)
                {
                    score -= score4;
                }
                else
                {
                    score -= score5;
                }
                HowManyTimes++;
                evaluate(depth + 1, 3, score);
                HowManyTimes--;
                for (int o = 0; o < s; ++o)
                {
                    Curmycard[wings[o]] = 2;
                }
                Cardnum += s * 2;
                if (i <= 7)
                {
                    score += score3;
                }
                else if (s == 1)
                {
                    score += score4;
                }
                else
                {
                    score += score5;
                }
            }
 
            //飞机不带翼
            if (i <= 7)
            {
                score -= score3;
            }
            else if (s == 1) //三张连续
            {
                score -= score4;
            }
            else
            {
                score -= score5;
            }
            HowManyTimes++;
            evaluate(depth + 1, 3, score);
            HowManyTimes--;
            if (i <= 7)
            {
                score += score3;
            }
            else if (s == 1)
            {
                score += score4;
            }
            else
            {
                score += score5;
            }
            for (int j = 0; j < s; ++j)
            {
                Curmycard[i + j] += 3;
            }
            Cardnum += 3 * s;
 
 
        }
    }
 
    case 4:
        //四带二
    {
        for (int i = 0; i <= 12; ++i)
        {
            if (Curmycard[i] == 4)
            {
                Curmycard[i] = 0;
                Cardnum -= 4;
                int _single[2], single = 0;
                int _pair[2], pair = 0;
                for (int t = 0; t < 12; ++t)
                {
                    if (Curmycard[t] == 1 && single < 2)
                    {
                        _single[single] = t;
                        single++;
                    }
                    else if (Curmycard[t] == 2 && pair < 2)
                    {
                        _pair[pair] = t;
                        pair++;
                    }
                }
                //四带二单张
                if (single == 2)
                {
                    Curmycard[_single[0]] = Curmycard[_single[1]] = 0;
                    Cardnum -= 2;
                    score -= score7;
                    HowManyTimes++;
                    evaluate(depth + 1, 4, score);
                    HowManyTimes--;
                    Curmycard[_single[0]] = Curmycard[_single[1]] = 1;
                    Cardnum += 2;
                    score += score7;
                }
                //四带二对子
                if (pair == 2)
                {
                    Curmycard[_pair[0]] = Curmycard[_pair[1]] = 0;
                    Cardnum -= 4;
                    score -= score8;
                    HowManyTimes++;
                    evaluate(depth + 1, 4, score);
                    HowManyTimes--;
                    Curmycard[_pair[0]] = Curmycard[_pair[1]] = 2;
                    Cardnum += 4;
                    score += score8;
                }
            }
        }
    }
 
    case 5:
        //对子 or 单牌
    {
        double t = score;
        int n = 0;
        for (int i = 0; i < MAX_LEVEL - 5; ++i)
        {
            if (Curmycard[i] != 0)
            {
                score -= score9;
                ++n;
            }
        }
 
        for (int i = MAX_LEVEL - 5; i < MAX_LEVEL - 3; ++i)//QKA
        {
            if (Curmycard[i] != 0)
            {
                score -= score10;
                ++n;
            }
        }
        for (int i = MAX_LEVEL - 3; i < MAX_LEVEL - 1; ++i)//2 and joker
        {
            if (Curmycard[i] != 0)
            {
                score -= score11;
                ++n;
            }
        }
        if (Curmycard[MAX_LEVEL - 1] != 0)
        {
            score -= score12;
            ++n;
        }
        HowManyTimes += n;
        evaluate(depth, 6, score);
        HowManyTimes -= n;
        score = t;
    }
    default:
        break;
    }
    return maxscore;
}
 
 
 
void addcard(Level l, short c)
{
    if (l == MAX_LEVEL - 2 || l == MAX_LEVEL - 1)
    {
        bestcard.push_back(54 - (MAX_LEVEL - l));
        return;
    }
    for (short i = l * 4; i < l * 4 + 4; ++i)
    {
        if (myCards.find(i) != myCards.end() && c > 0)
        {
            bestcard.push_back(Card(i));
            c--;
        }
    }
    return;
}
 
 //----------Dzj-----------------------------------------------------------//
CardCombo findBestValld()
{
   if (lastValidCombo.comboType == CardComboType::ROCKET)//先看看别人是不是王炸
       return CardCombo();
   //
   if (lastValidCombo.comboType == CardComboType::PASS)  //上一手牌是过
       last_player_is_who = myPosition;
   else if (whatTheyPlayed[(myPosition + 2) % 3].back().size() != 0)//我的上家有没有打牌？
   {
       last_player_is_who = (myPosition + 2) % 3; //上家的编号
   }
   else//我的上家没有打牌
   {
       last_player_is_who = (myPosition + 1) % 3;//上一手牌 是我的下家打的
   }
//以上代码的目的，判断上一手牌是队友还是对手打的，还是自己打的。
   bestcard.clear();
   evaluate_init();
   double value = 1e9;
   double score = 0;
   double cur_score = evaluate(0, 0, score);    //评估函数,用来评价当前的手牌质量，分数越低手牌质量越好

    if (last_player_is_who == myPosition){
        if ((myPosition == 0 && (cardRemaining[1] <= 2 || cardRemaining[2] <= 2))
            || (myPosition == 1 && cardRemaining[0] <= 2) || (myPosition == 2 && cardRemaining[0] <= 2)){ //轮到我出牌，对手的牌剩下一张或者两张
            if (Cardnum == 1){
                bestcard.clear();
                for (int i = 14; i >= 0; --i){
                    if (Curmycard[i]){
                        addcard(i, 1);
                        return CardCombo(bestcard.begin(), bestcard.end());
                    }
                }
            }
            else if (Cardnum == 2){
                bestcard.clear();
                for (int i = 14; i >= 0; --i){
                    if (Curmycard[i] == 2){
                        addcard(i, 2);
                        return CardCombo(bestcard.begin(), bestcard.end());
                    }
                    else if (Curmycard[i] == 1){
                        addcard(i, 1);
                        return CardCombo(bestcard.begin(), bestcard.end());
                    }
                }
            }
            else{
                //出三带或者飞机
                int s = 0;
                evaluate_init();
                int begin = 0;
                while (true){
                    int k = find_straight(s, 3, 1, begin);
                    if (s == 0)
                        break;
                    else{
                        evaluate_init();
                        for (int j = 0; j < s; ++j)
                            Curmycard[k+j] -= 3;
                        Cardnum -= 3*s;
                        int wings[15], pair = 0;
                        int wing[15], single = 0;
                        for (int t = 0; t < MAX_LEVEL && (t < k || t >= k + s); ++t){
                            if (Curmycard[t] == 1)
                            {
                                wing[single] = t;
                                single++;
                            }
                            else if (Curmycard[t] == 2)
                            {
                                wings[pair] = t;
                                pair++;
                            }
                        }
                        Cardnum += 3*s;
                        for (int j = 0; j < s; ++j)
                            Curmycard[k+j] += 3;
                        bestcard.clear();
                        for (int t = k; t < k+s; ++t)
                            addcard(t, 3);
                        if (single >= s){
                            for (int o = 0; o < s; ++o)
                                addcard(wing[o], 1);
                            //Cardnum -= s;
                            return CardCombo(bestcard.begin(), bestcard.end());
                        }
                        if (pair >= s){
                            for (int o = 0; o < s; ++o)
                                addcard(wings[o], 2);
                            //Cardnum -= s*2;
                            return CardCombo(bestcard.begin(), bestcard.end());
                        }
                    }
                    begin = k + s;
                }
                if (!bestcard.empty())
                    return CardCombo(bestcard.begin(), bestcard.end());
                //顺子
                evaluate_init();
                s = 0;
                int k = find_straight(s, 1, 5, 0);
                bestcard.clear();
                if (s >= 5){
                    for (int b = k; b < k+s; ++b) //这里是有的顺子全部出了
                        addcard(b, 1);
                    //Cardnum -= s;
                    return CardCombo(bestcard.begin(), bestcard.end());
                }
                //连对
                evaluate_init();
                s = 0;
                k = find_straight(s, 2, 3, 0);
                bestcard.clear();
                if (s >= 3){
                    for (int b = k; b < k+s; ++b)
                        addcard(b, 2);
                    //Cardnum -= 2*s;
                    return CardCombo(bestcard.begin(), bestcard.end());
                }
                //四带二
                bestcard.clear();
                for (int i = 0; i <= 12; ++i){
                    if (Curmycard[i] == 4){
                        if (Cardnum == 6){
                            addcard(i, 4);
                            Curmycard[i] = 0;
                            Cardnum -= 4;
                            for (int j = 0; j <= 14; ++j){
                                if (Curmycard[j] == 1)
                                    addcard(j, 1);
                                else if (Curmycard[j] == 2)
                                    addcard(j, 2);
                            }
                            Curmycard[i] = 4;
                            Cardnum += 4;
                            return CardCombo(bestcard.begin(), bestcard.end());
                        }
                        else if (Cardnum == 4){
                            if (Cardnum == 4){
                                addcard(i, 4);
                                return CardCombo(bestcard.begin(), bestcard.end());
                            }
                        }
                    }
                }
                //如果对方只剩单张，就先出双
                bestcard.clear();
                if ((myPosition == 0 && (cardRemaining[1] == 1 || cardRemaining[2] == 1))
                    || (myPosition == 1 && cardRemaining[0] == 1) || (myPosition == 2 && cardRemaining[0] == 1)){
                    for (int i = 1; i <=12; i++){
                        if (Curmycard[i] == 2){
                            addcard(i, 2);
                            return CardCombo(bestcard.begin(), bestcard.end());
                        }
                    }
                }
                //这些都没有，就从大到小出单吧
                bestcard.clear();
                if (Cardnum == 2 && (Curmycard[13] == 1 || Curmycard[14] == 1)){
                    if (Curmycard[13]){
                        addcard(13, 1);
                        return CardCombo(bestcard.begin(), bestcard.end());
                    }
                    else{
                        addcard(14, 1);
                        return CardCombo(bestcard.begin(), bestcard.end());
                    }
                }
                if (Cardnum == 3 && Curmycard[13] == 1 && Curmycard[14] == 1){
                    addcard(13, 1);
                    addcard(14, 1);
                    return CardCombo(bestcard.begin(), bestcard.end());
                }
                for (int i = 12; i >= 0; --i){ //不是王的单张先出了
                    if (Curmycard[i] == 1){
                        //Cardnum--;
                        addcard(i, 1);
                        return CardCombo(bestcard.begin(), bestcard.end());
                    }
                }
                bestcard.clear();
                if (Cardnum == 2 && Curmycard[13] == 1 && Curmycard[14] == 1){ //如果有王炸就出
                    addcard(13, 1);
                    addcard(14, 1);
                    //Cardnum = 0;
                    return CardCombo(bestcard.begin(), bestcard.end());
                }
                for (int i = 13; i <= 14; ++i){
                    if (Curmycard[i]){
                        //Cardnum--;
                        addcard(i, 1);
                        return CardCombo(bestcard.begin(), bestcard.end());
                    }
                }
            }
        }
    }
    else{
        if (lastValidCombo.comboType == CardComboType::SINGLE){ //接单张
            if ((myPosition == 0 && (cardRemaining[1] <= 2 || cardRemaining[2] <= 2))
                || (myPosition == 1 && cardRemaining[0] <= 2) || (myPosition == 1 && cardRemaining[0] <= 2)){
                    bestcard.clear();
                    if (Cardnum == 2 && (Curmycard[13] == 1 || Curmycard[14] == 1)){ //只有单张和一张王
                        if (Curmycard[13]){
                            addcard(13, 1);
                            return CardCombo(bestcard.begin(), bestcard.end());
                        }
                        else{
                            addcard(14, 1);
                            return CardCombo(bestcard.begin(), bestcard.end());
                        }
                    }
                    if (Cardnum == 3 && Curmycard[13] == 1 && Curmycard[14] == 1){ //单张和两张王
                        addcard(13, 1);
                        addcard(14, 1);
                        return CardCombo(bestcard.begin(), bestcard.end());
                    }
                    for (int i = 12; i > lastValidCombo.comboLevel; --i){
                        if (Curmycard[i] == 1){
                            addcard(i, 1);
                            return CardCombo(bestcard.begin(), bestcard.end());
                        }
                    }
                    for (int i = 12; i > lastValidCombo.comboLevel; --i){
                        if (Curmycard[i] == 2){
                            addcard(i, 1);
                            return CardCombo(bestcard.begin(), bestcard.end());
                        }
                    }
                    for (int i = 14; i >= 13 && i > lastValidCombo.comboLevel; --i){ //两张王拆开出
                        if(Curmycard[i]){
                            addcard(i, 1);
                            return CardCombo(bestcard.begin(), bestcard.end());
                        }
                    }
                }
        }
        else if (lastValidCombo.comboType == CardComboType::PAIR){ //接对子
            if ((myPosition == 0 && (cardRemaining[1] <= 3 || cardRemaining[2] <= 3))
                || (myPosition == 1 && cardRemaining[0] <= 3) || (myPosition == 2 && cardRemaining[0] <= 3)){
                    for (int i = 12; i > lastValidCombo.comboLevel; --i){ //刚好出两张
                        if (Curmycard[i] == 2){
                            addcard(i, 2);
                            return CardCombo(bestcard.begin(), bestcard.end());
                        }
                    }
                    for (int i = 12; i > lastValidCombo.comboLevel; --i){ //拆三张
                        if (Curmycard[i] == 3){
                            addcard(i, 2);
                            return CardCombo(bestcard.begin(), bestcard.end());
                        }
                    }
                }
        }
    }

   if (lastValidCombo.comboType != CardComboType::PASS)
   {
       //有限制的出牌
       int i = 0;
       //必须出单张
       if (lastValidCombo.comboType == CardComboType::SINGLE)
       {
           evaluate_init();
           for (int i = lastValidCombo.comboLevel + 1; i < MAX_LEVEL; ++i)
           {
               if (Curmycard[i] != 0)
               {
                   evaluate_init();
                   Curmycard[i]--;
                   Cardnum--;
                   double score = 0;
                   double temp = evaluate(0, 0, score);
                   temp += extra_point(1, i);
                   if (Cardnum == 0)
                       temp = -1e9;
                   Curmycard[i]++;
                   Cardnum++;
                   if (value > temp)
                   {
                       value = temp;
                       bestcard.clear();
                       addcard(i, 1);
                   }
               }
           }
       }
       //必须出对子
       else if (lastValidCombo.comboType == CardComboType::PAIR)
       {
           evaluate_init();
           for (int i = lastValidCombo.comboLevel + 1; i < MAX_LEVEL - 2; ++i)
           {
               if (Curmycard[i] >= 2)
               {
                   evaluate_init();
                   Curmycard[i] -= 2;
                   Cardnum -= 2;
                   double score = 0;
                   double temp = evaluate(0, 0, score);
                   temp += extra_point(2, i);
                   if (Cardnum == 0)
                       temp = -1e9;
                   Curmycard[i] += 2;
                   Cardnum += 2;
                   if (value > temp)
                   {
                       value = temp;
                       bestcard.clear();
                       addcard(i, 2);
                   }
               }
           }
       }

       //是单顺或双顺
       else if (lastValidCombo.comboType == CardComboType::STRAIGHT || lastValidCombo.comboType == CardComboType::STRAIGHT2)
       {
           evaluate_init();
           int c;
           if (lastValidCombo.comboType == CardComboType::STRAIGHT)//单顺
               c = 1;
           else//双顺
               c = 2;
           int s = lastValidCombo.cards.size() / c;   //顺子的牌的数量大小
           for (int i = lastValidCombo.comboLevel + 1; i <= MAX_LEVEL - 3 - s; ++i)
           {
               int j = 0;
               while (i + j <= MAX_LEVEL - 3 && Curmycard[i + j] >= c)
               {
                   j++;
               }
               if (j >= s)
               {
                   evaluate_init();
                   for (int k = 0; k <= j - s; ++k)
                   {
                       for (int t = k; t < s + k; ++t)
                       {
                           Curmycard[i + t] -= c;
                       }
                       Cardnum -= s * c;
                       double score = 0;
                       double temp = evaluate(0, 0, score);
                       temp += extra_point(5, i);
                       if (Cardnum == 0)
                           temp = -1e9;
                       for (int t = k; t < s + k; ++t)
                       {
                           Curmycard[i + t] += c;
                       }
                       Cardnum += s * c;
                       if (value > temp)
                       {
                           value = temp;
                           bestcard.clear();
                           for (int t = k; t < s + k; ++t)
                           {
                               addcard(i + t, c);
                           }
                       }
                   }
               }
               i = i + j + 1;
           }
       }

       //三带 （0，1，2）；飞机
       else if (lastValidCombo.comboType == CardComboType::TRIPLET || lastValidCombo.comboType == CardComboType::TRIPLET2 || lastValidCombo.comboType == CardComboType::TRIPLET1 ||
           lastValidCombo.comboType == CardComboType::PLANE || lastValidCombo.comboType == CardComboType::PLANE2 || lastValidCombo.comboType == CardComboType::PLANE1)
       {
           evaluate_init();
           int c, s;
           if (lastValidCombo.comboType == CardComboType::TRIPLET || lastValidCombo.comboType == CardComboType::TRIPLET2 || lastValidCombo.comboType == CardComboType::TRIPLET1)//三带
           {
               c = lastValidCombo.cards.size() - 3;//c=0,1,2
               s = 1;
           }
           else//飞机
           {
               if (lastValidCombo.packs.back().count != 3)
                   s = lastValidCombo.packs.size() / 2;
               else
                   s = lastValidCombo.packs.size();
               c = (lastValidCombo.cards.size() - s * 3) / s;
           }

           for (int i = lastValidCombo.comboLevel + 2 - s; i <= MAX_LEVEL - 2 - s;)
           {
               int j = 0;
               while (i + j < MAX_LEVEL - 2 && Curmycard[i + j] == 3)
               {
                   j++;
               }
               if (j >= s)//找到了三带（飞机）
               {
                   for (int t = 0; t <= j - s; ++t)
                   {
                       evaluate_init();
                       int sc[4], single = 0;
                       for (int k = i + t; k < i + s + t; ++k)
                       {
                           Curmycard[k] -= 3;
                           Cardnum -= 3;
                       }
                       for (int k = 0; k < MAX_LEVEL; ++k)
                       {
                           if (c == 0 || single == s)
                               break;
                           if (Curmycard[k] == c && single < s && (k < i + t || k >= i + s + t))
                           {
                               sc[single] = k;
                               single++;
                               Curmycard[single] -= c;
                               Cardnum -= c;
                           }
                       }
                       if (c != 0 && single < s)//找带的牌,实在不行就拆牌
                       {
                           for (int k = 0; k < MAX_LEVEL; ++k)
                           {
                               if (c == 0)
                                   break;
                               if (Curmycard[k] >= c && single < s && (k < i + t || k >= i + s + t))
                               {
                                   sc[single] = k;
                                   single++;
                                   Curmycard[single] -= c;
                                   Cardnum -= c;
                               }
                           }
                       }
                       if (c != 0 && single < s)//拆牌都不行
                       {
                           break;
                       }

                       double score = 0;
                       double temp = evaluate(0, 0, score);
                       temp += extra_point(4, i);
                       if (Cardnum == 0)//能一下出完
                           temp = -1e9;
                       if (value > temp)
                       {
                           value = temp;
                           bestcard.clear();
                           for (int k = i + t;k < s + i + t; ++k)
                           {
                               addcard(k, 3);
                           }
                           if(c != 0)
                               for (int t = 0; t < s; ++t)
                               {
                                   addcard(sc[t], c);
                               }
                       }
                       for (int k = i + t; k < i + s + t; ++k)
                       {
                           Curmycard[k] += 3;
                           Cardnum += 3;
                       }
                   }
               }
               i = i + j + 1;
           }

       }
       //四带二（带两个或者两对）
       else if (lastValidCombo.comboType == CardComboType::QUADRUPLE2 || lastValidCombo.comboType == CardComboType::QUADRUPLE4)
       {
           int c;
           //evaluate_prepare();
           if (lastValidCombo.comboType == CardComboType::QUADRUPLE2)
           {
               c = 1;       //是带两个，
           }
           else
               c = 2;
           for (int i = lastValidCombo.comboLevel + 1; i < MAX_LEVEL; ++i)//似乎有问题？寻找过程中删掉的牌并没有加上
           {
               if (Curmycard[i] == 4)
               {
                   evaluate_init();
                   Curmycard[i] -= 4;
                   Cardnum -= 4;
                   int sc[2], single = 0;
                   for (int k = 0; k < MAX_LEVEL - 3; ++k)  
                   {
                       if (Curmycard[k] == c && single < 2)
                       {
                           sc[single] = k;
                           single++;
                           Curmycard[k] -= c;
                           Cardnum -= c;
                       }
                   }
                   if (single < 2)
                   {
                       i = MAX_LEVEL;
                       break;
                   }

                   double score = 0;
                   double temp = evaluate(0, 0, score);
                   temp += extra_point(6, i);
                   if (Cardnum == 0)//能一下出完
                       temp = -1e9;
                   if (value > temp)
                   {
                       bestcard.clear();
                       value = temp;
                       for (int j = 0; j < 4; ++j)
                           bestcard.push_back(Card(4 * i + j));
                       for (int j = 0; j < 2; ++j)
                           addcard(sc[j], c);
                       break;
                   }
               }
           }
       }
       //之前是炸弹or任意牌也可出炸弹
       if (lastValidCombo.comboType == CardComboType::BOMB)
       {
           for (i = lastValidCombo.comboLevel + 1 ; i < MAX_LEVEL - 2; ++i)//，炸弹；找出大过，且让手牌价值变最小，的炸弹
           {
               evaluate_init();
               if (Curmycard[i] == 4)
               {
                   Curmycard[i] = 0;
                   Cardnum -= 4;
                   double score = 0;
                   double temp = evaluate(0, 0, score);
                   Curmycard[i] = 4;
                   Cardnum += 4;
                   if (value > temp)
                   {
                       bestcard.clear();
                       value = temp;
                       for (int j = 0; j < 4; ++j)
                           bestcard.push_back(Card(4 * i + j));
                   }
               }
           }
       }
       if(bestcard.empty())
       {
           for (i = 0 ; i < MAX_LEVEL - 2; ++i)//炸弹
           {
               if (Curmycard[i] == 4)
               {
                   for (int j = 0; j < 4; ++j)
                       bestcard.push_back(Card(4 * i + j));
                   break;
               }
           }
       }
       //火箭
       if ((Curmycard[MAX_LEVEL - 2] == 1 && Curmycard[MAX_LEVEL - 1] == 1)&&(bestcard.empty()))
       {
           evaluate_init();
           Curmycard[MAX_LEVEL - 2] = Curmycard[MAX_LEVEL - 1] = 0;
           Cardnum -= 2;
           double score = 0;
           double temp = evaluate(0, 0, score);
           Curmycard[MAX_LEVEL - 2] = Curmycard[MAX_LEVEL - 1] = 1;
           Cardnum += 2;
           if (value > temp)
           {
               bestcard.clear();
               value = temp;
               for (int j = 0; j < 2; ++j)
                   bestcard.push_back(Card(52 + j));
           }
       }
       if(bestcard.empty())
       {
           if (lastValidCombo.comboType == CardComboType::SINGLE)
           {
               if(Curmycard[12]!=0 && lastValidCombo.comboLevel<12)
                   addcard(12, 1);
               else if(lastValidCombo.comboLevel==13 && Curmycard[14]!=0)
                   addcard(14, 1);
           }
           else if (lastValidCombo.comboType == CardComboType::PAIR)
           {
               if(Curmycard[12]>=2 && lastValidCombo.comboLevel<12)
                   addcard(12, 2);
           }
       }

       
       
   }
   else
   {
       //没有限制的出牌
       //特殊情况：最后只剩炸弹or火箭
       switch(0)
       {
           case 0:
           {
               for (int i=0; i < MAX_LEVEL - 2; ++i)//炸弹
               {
                   evaluate_init();
                   if (Curmycard[i] == 4)
                   {
                       Curmycard[i] = 0;
                       Cardnum -= 4;
                       if (Cardnum == 0)//能一下出完
                       {
                           for (int j = 0; j < 4; ++j)
                               bestcard.push_back(Card(4 * i + j));
                           Curmycard[i] = 4;
                           Cardnum += 4;
                           break;
                       }
                   }
                   if(!bestcard.empty())
                       break;
               }
           }
           case 1:
           {
               if ((Curmycard[MAX_LEVEL - 2] == 1 && Curmycard[MAX_LEVEL - 1] == 1))  //最后只剩火箭
               {
                   if(Cardnum==2)
                   {
                       for (int j = 0; j < 2; ++j)
                           bestcard.push_back(Card(52 + j));
                       break;
                   }
               }
           }
           
       //出三带x或者飞机-------------------------------------------能否对带的牌全部搜索？
       case 2:
       {
           int s = 0;
           evaluate_init();
           int begin = 0;
           while (true)
           {
               int i = find_straight(s, 3, 1, begin);
               //有三个头
               if (s == 0)
                   break;
               else
               {
                   evaluate_init();
                   for (int j = 0; j < s; ++j)
                   {
                       Curmycard[i + j] -= 3;
                   }
                   Cardnum -= 3 * s;
                   int wings[15], pair = 0;
                   int wing[15], single = 0;
                   for (int t = 0; t < MAX_LEVEL && (t < i || t >= i + s); ++t)
                   {
                       if (Curmycard[t] == 1)
                       {
                           wing[single] = t;
                           single++;
                       }
                       else if (Curmycard[t] == 2)
                       {
                           wings[pair] = t;
                           pair++;
                       }
                   }
                   
                   //飞机不带翼
                   double score = 0;
                   double temp = evaluate(0, 0, score);
                   if (i <= 7 && s >= 2)
                       temp = temp - 0.5 + extra_point(3, i);//根据i的大小来出牌
                   else if (i <= 7)
                       temp = temp - 0.3 + extra_point(3, i);
                   else
                       temp -= 0.1 + extra_point(6, i);
                   if (Cardnum == 0)//只有一把出完，才不带翼
                   {
                       temp = -1e9;

                       if (value > temp)
                       {
                           value = temp;
                           bestcard.clear();
                           for (int t = i; t < i + s; ++t)
                           {
                               addcard(t, 3);
                           }
                       }
                   }

                   //飞机带小翼
                   if (single >= s)
                   {
                       for (int o = 0; o < s; ++o)
                       {
                           Curmycard[wing[o]] = 0;
                       }
                       Cardnum -= s;
                       double score = 0;
                       double temp = evaluate(0, 0, score);
                       if (i <= 7 && s >= 2)
                       {
                           if(i==0)
                               temp = -1e10;
                           else
                               temp = temp - 5 + extra_point(3, i);//根据i的大小来出牌
                       }
                       else if (i <= 7)
                       {
                           if(i==0)
                               temp = -1e10;
                           else
                               temp = temp - 4 + extra_point(3, i);//根据i的大小来出牌
                       }
                       else
                           temp -= 1 + extra_point(6, i);
                       if (Cardnum == 0 )//能一下出完，太棒了！
                           temp = -1e9;
                       for (int o = 0; o < s; ++o)
                       {
                           Curmycard[wing[o]] = 1;
                       }
                       Cardnum += s;
                       if (value > temp)
                       {
                           value = temp;
                           bestcard.clear();
                           for (int t = i; t < i + s; ++t)
                           {
                               addcard(t, 3);
                           }
                           for (int t = 0; t < s; ++t)
                           {
                               addcard(wing[t], 1);
                           }
                       }
                   }

                   //飞机带大翼
                   if (pair >= s)
                   {
                       for (int o = 0; o < s; ++o)
                       {
                           Curmycard[wings[o]] = 0;
                       }
                       Cardnum -= s * 2;
                       double score = 0;
                       double temp = evaluate(0, 0, score);
                       if (i <= 7 && s >= 2)
                       {
                           if(i==0)
                               temp = -1e10;
                           else
                               temp = temp - 5 + extra_point(3, i);//根据i的大小来出牌
                       }
                       else if (i <= 7)
                       {
                           if(i==0)
                               temp = -1e10;
                           else
                               temp = temp - 4 + extra_point(3, i);//根据i的大小来出牌
                       }
                       else
                           temp -= 1 + extra_point(6, i);
                       if (Cardnum == 0 )
                           temp = -1e9;
                       for (int o = 0; o < s; ++o)
                       {
                           Curmycard[wings[o]] = 2;
                       }
                       Cardnum += s * 2;
                       for (int j = 0; j < s; ++j)
                       {
                           Curmycard[i + j] += 3;
                       }
                       Cardnum += 3 * s;
                       if (value > temp)
                       {
                           value = temp;
                           bestcard.clear();
                           for (int t = i; t < i + s; ++t)
                           {
                               addcard(t, 3);
                           }
                           for (int t = 0; t < s; ++t)
                           {
                               addcard(wings[t], 2);
                           }
                       }
                   }

               }
               begin = i + s;
               
           }
       }

       //出顺子吗
       case 4:
       {
           evaluate_init();
           int s = 0;
           int i = find_straight(s, 1, 5, 0);//1表示单顺
           if (s >= 5)
           {
               for (int b = i; b <= i + s - 5; ++b)
               {
                   for (int k = 5; k <= s && b + k <= i + s; ++k)
                   {
                       for (int j = 0; j < k; ++j)
                       {
                           Curmycard[b + j]--;
                       }
                       Cardnum -= s;
                       double score = 0;
                       double temp = evaluate(0, 0, score);
                       temp += extra_point(5, b);
                       if (b + s < 11)
                           temp = temp - 0.8;//根据i的大小来出牌, 出顺子加0.5分
                       else
                           temp = temp - 0.5;
                       if (Cardnum == 0)//能一下出完
                           temp = -1e9;
                       for (int j = 0; j < k; ++j)
                       {
                           Curmycard[b + j]++;
                       }
                       Cardnum += s;
                       if (value > temp)
                       {
                           value = temp;
                           bestcard.clear();
                           for (int t = 0; t < s; ++t)
                           {
                               addcard(i + t, 1);
                           }
                       }
                   }
               }
           }
       }
       case 5:
       //出连对
       {
           evaluate_init();
           int s = 0;
           int i = find_straight(s, 2, 3, 0);
           if (s >= 3)
           {
               for (int j = 0; j < s; ++j)
               {
                   Curmycard[i + j] -= 2;
               }
               Cardnum -= s * 2;
               double score = 0;
               double temp = evaluate(0, 0, score);
               temp = temp - (15 - i) * 1.0 / 100 - 0.5;//根据i的大小来出牌
               if (Cardnum == 0)//能一下出完，太棒了！
                   temp = -1e9;
               for (int j = 0; j < s; ++j)
               {
                   Curmycard[i + j] += 2;
               }
               Cardnum += s * 2;
               if (value > temp)
               {
                   value = temp;
                   bestcard.clear();
                   for (int t = 0; t < s; ++t)
                   {
                       addcard(i + t, 2);
                   }
               }
           }
       }
       case 6:
       //出对子
       {
           evaluate_init();
           for (int i = 0; i < MAX_LEVEL; ++i)
           {

               if (Curmycard[i] >= 2)
               {
                   evaluate_init();
                   Curmycard[i] -= 2;
                   Cardnum -= 2;
                   double score = 0;
                   double temp = evaluate(0, 0, score);
                   temp += extra_point(2, i) - 0.05 + i / 200.0;
                   if (Cardnum == 0)
                       temp = -1e10;
                   Curmycard[i] += 2;
                   Cardnum += 2;
                   if (value > temp)
                   {
                       value = temp;
                       bestcard.clear();
                       addcard(i, 2);
                   }
               }
           }
       }
           case 7:
       //有单张出单张
       {
           
           evaluate_init();
           for (int i = 0; i < MAX_LEVEL; ++i)
           {
               if (Curmycard[i] != 0)
               {
                   evaluate_init();
                   Curmycard[i]--;
                   Cardnum--;
                   double score = 0;
                   double temp = evaluate(0, 0, score);
                   temp += extra_point(1, i) - 0.05 + i / 200.0;
                   Curmycard[i]++;
                   Cardnum++;
                   if (value > temp)
                   {
                       value = temp;
                       bestcard.clear();
                       addcard(i, 1);
                   }
               }
           }
       }

           case 8:
       //出四带二
       {
           evaluate_init();
           for (int i = 0; i <= 12; ++i)
           {
               if (Curmycard[i] == 4)
               {
                   evaluate_init();
                   Curmycard[i] = 0;
                   Cardnum -= 4;
                   int _single[2], single = 0;
                   int _pair[2], pair = 0;
                   for (int t = 0; t < 12; ++t)
                   {
                       if (Curmycard[t] == 1 && single < 2)
                       {
                           _single[single] = t;
                           single++;
                       }
                       else if (Curmycard[t] == 2 && pair < 2)
                       {
                           _pair[pair] = t;
                           pair++;
                       }
                   }
                   //四带二单张
                   if (single == 2)
                   {
                       Curmycard[_single[0]] = Curmycard[_single[1]] = 0;
                       Cardnum -= 2;
                       double score = 0;
                       double temp = evaluate(0, 0, score);
                       temp += extra_point(5, i);
                       if (Cardnum == 0)
                           temp = -1e9;
                       Curmycard[_single[0]] = Curmycard[_single[1]] = 1;
                       Cardnum += 2;
                       if (value > temp)
                       {
                           bestcard.clear();
                           value = temp;
                           for (int j = 0; j < 4; ++j)
                               bestcard.push_back(Card(4 * i + j));
                           for (int j = 0; j < 2; ++j)
                               addcard(_single[j], 1);
                       }
                   }
                   //四带二对子
                   if (pair == 2)
                   {
                       Curmycard[_pair[0]] = Curmycard[_pair[1]] = 0;
                       Cardnum -= 4;
                       double score = 0;
                       double temp = evaluate(0, 0, score);
                       temp += extra_point(6, i);
                       if (Cardnum == 0)
                           temp = -1e9;
                       Curmycard[_pair[0]] = Curmycard[_pair[1]] = 2;
                       Cardnum += 4;
                       if (value > temp)
                       {
                           bestcard.clear();
                           value = temp;
                           for (int j = 0; j < 4; ++j)
                               bestcard.push_back(Card(4 * i + j));
                           for (int j = 0; j < 2; ++j)
                               addcard(_pair[j], 2);
                       }
                   }
               }
           }
       }
       }
   }

    if (last_player_is_who == myPosition)//自己前一把打的牌没人要
    {
        last_player_is_friend_ornot=2;
        return CardCombo(bestcard.begin(), bestcard.end());
    }
    else if ((last_player_is_who+myPosition )== 3)//队友出的牌
    {
        last_player_is_friend_ornot=1;
        if(lastValidCombo.comboLevel>10 && (Cardnum-bestcard.size())>2)    //不压队友的大牌，除非自己要出完了；
            return CardCombo();
        else if(Cardnum-bestcard.size()==0)
            return CardCombo(bestcard.begin(), bestcard.end());
        else if (value >= cur_score || cardRemaining[last_player_is_who] <= 3)
        {
            if(last_player_is_who==((myPosition+1)%3))
                return CardCombo();
            else if (CardCombo(bestcard.begin(), bestcard.end()).comboLevel<9 &&CardCombo(bestcard.begin(), bestcard.end()).comboType!=CardComboType::BOMB )
                return CardCombo(bestcard.begin(), bestcard.end());
            else 
                return CardCombo();
                
        }
        else
            return CardCombo(bestcard.begin(), bestcard.end());
    }
    else  //对手出的牌
    {
        last_player_is_friend_ornot=0;
        if (myPosition == 0 && (value - cur_score >= 4) && cardRemaining[last_player_is_who] >= 5 && cardRemaining[3-last_player_is_who]>=5)
            return CardCombo();
        else if (value - cur_score >= 1 && cardRemaining[last_player_is_who] > 5 && myPosition == 1)
            return CardCombo();
        else if (value - cur_score >= 1 && cardRemaining[last_player_is_who] > 5 && myPosition == 2)
            return CardCombo();
        else
            return CardCombo(bestcard.begin(), bestcard.end());
    }
}

//----------Dzj-----------------------------------------------------------//


class BotzoneIO
{
public:
    void Input()
    {
        
        string line;
        getline(cin, line);
        Json::Value input;
        Json::Reader reader;
        reader.parse(line, input);
        
        int turn = input["requests"].size();
        assert(turn != 0);
        auto firstTurn = input["requests"][0u];
        auto curTurn = input["requests"][unsigned(turn - 1)];
        auto own = firstTurn["own"];
        Json::Value publicCard;
        for(int i=0; i<own.size(); ++i)
        {
            myCards.insert(own[i].asInt());
        }
        /* 叫分阶段 */
        if (!curTurn["bid"].isNull())
        {
            // 如果还可以叫分，则记录叫分
            auto bidHistory = curTurn["bid"];
            for (unsigned i = 0; i < bidHistory.size(); i++)
                bidInput.push_back(bidHistory[i].asInt());
        }
        
        int whoInHistory[2]; // 上上家和上家的位置
        
        /* 出牌阶段 */
        for(int i=0; i<turn; ++i)
        {
            auto request = input["requests"][i];
            /* 第一轮出牌初始化参数 */
            if(!request["publiccard"].isNull())
            {
                stage = Stage::PLAYING;
                publicCard = request["publiccard"];
                int landlordPosition = request["landlord"].asInt();
//                cardRemaining[landlordPosition] += publicCard.size();
                cardRemaining[0] += 3; // 地主加牌
                myPosition = (request["pos"].asInt() - landlordPosition + PLAYER_COUNT) % PLAYER_COUNT; // 设置地主为0号位
                whoInHistory[0] = (myPosition - 2 + PLAYER_COUNT) % PLAYER_COUNT;
                whoInHistory[1] = (myPosition - 1 + PLAYER_COUNT) % PLAYER_COUNT;
                landlordBid= request["finalbid"].asInt();
                for(int i=0; i<publicCard.size(); ++i)
                {
                    landlordPublicCards.insert(publicCard[i].asInt());
//                    if(landlordPosition == myPosition)
                    if(myPosition == 0) // 我为地主
                    {
                        myCards.insert(publicCard[i].asInt());
                    }
                }
            }
            
            /* 逐次恢复局面到当前 */
            auto history = input["requests"][i]["history"]; // 每个历史中有上家和上上家出的牌
            if (history.isNull())  continue;
            stage = Stage::PLAYING;
            
            
            int howManyPass = 0;
            for (int p = 0; p < 2; p++)
            {
                int player = whoInHistory[p]; // 是谁出的牌
                auto history_Action = history[p]; // 出的哪些牌
//                cout << "player: " << player << " size:" << history_Action.size();
                vector<Card> playedCards;
                for (unsigned index = 0; index < history_Action.size(); index++) // 循环枚举这个人出的所有牌
                {
                    int card = history_Action[index].asInt(); // 这里是出的一张牌
                    playedCards.push_back(card);
                }
                whatTheyPlayed[player].push_back(playedCards); // 记录这段历史
                cardRemaining[player] -= history_Action.size();
 
                if (history_Action.size() == 0)
                    howManyPass++;
                else
                    lastValidCombo = CardCombo(playedCards.begin(), playedCards.end());
            }
 
            if (howManyPass == 2)
                lastValidCombo = CardCombo();
 
            if (i < turn - 1)
            {
                // 还要恢复自己曾经出过的牌
                auto history_Action = input["responses"][i]; // 出的哪些牌
                vector<Card> playedCards;
                for (unsigned index = 0; index < history_Action.size(); index++) // 循环枚举自己出的所有牌
                {
                    int card = history_Action[index].asInt(); // 这里是自己出的一张牌
                    myCards.erase(card); // 从自己手牌中删掉
                    playedCards.push_back(card);
                }
                whatTheyPlayed[myPosition].push_back(playedCards); // 记录这段历史
                cardRemaining[myPosition] -= history_Action.size();
            }
        }
    }
    
    
    void bid(int value)
    {
        Json::Value result;
        result["response"] = value;
        
        Json::FastWriter writer;
        cout << writer.write(result) << endl;
    }
    
    /*
     * 输出打牌决策，begin是迭代器起点，end是迭代器终点
     * CARD_ITERATOR是Card（即short）类型的迭代器
     */
    template <typename CARD_ITERATOR>
    void play(CARD_ITERATOR begin, CARD_ITERATOR end)
    {
        Json::Value result, response(Json::arrayValue);
        for (; begin != end; begin++)
            response.append(*begin);
        result["response"] = response;

        Json::FastWriter writer;
        cout << writer.write(result) << endl;
    }
};

//构造一个remain_card的集合，里面是对面出剩的牌
void insert_all_card()
{
    for (int i = 0; i < 53; ++i)
        remain_card.insert(Card(i));
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < whatTheyPlayed[i].size(); ++j)
            for (int k = 0; k < whatTheyPlayed[i][j].size(); ++k)
            {
                remain_card.erase(whatTheyPlayed[i][j][k]);
            }
    }
    for (auto i = myCards.begin(); i != myCards.end(); ++i)
    {
        remain_card.erase(*i);
    }
    return;
}

//叫分决策
int point(int maxBidit){
    int val=Curmycard[11]*0.5+Curmycard[12]+(Curmycard[13]+Curmycard[14])*1.5;
    for (int i=0; i < MAX_LEVEL - 4; ++i)//，炸弹
    {
        if(Curmycard[i]==4)
            val+=1;
        else if(Curmycard[i]==3)
            val+=0.5;
    }
    if(maxBidit==-1 && val>=5)
        return 3;
    else 
        return maxBidit+1;
}
 
int main()
{
    srand(time(nullptr));
    BotzoneIO IO;
    IO.Input();
    
    if (stage == Stage::BIDDING)
    {
        // 做出决策（你只需修改以下部分）

        auto maxBidIt = std::max_element(bidInput.begin(), bidInput.end());
        int maxBid = maxBidIt == bidInput.end() ? -1 : *maxBidIt;       //判断有没有已知叫分，

        int bidValue = point(maxBid);
        // 决策结束，输出结果（你只需修改以上部分）

        IO.bid(bidValue);
    }
    else
    {
        insert_all_card();
        // 是合法牌
        CardCombo myAction = CardCombo();
        try {
            myAction = findBestValld();
            assert(myAction.comboType != CardComboType::INVALID);
     
            assert(
                // 在上家没过牌的时候过牌
                (lastValidCombo.comboType != CardComboType::PASS && myAction.comboType == CardComboType::PASS) ||
                // 在上家没过牌的时候出打得过的牌
                (lastValidCombo.comboType != CardComboType::PASS && lastValidCombo.canBeBeatenBy(myAction)) ||
                // 在上家过牌的时候出合法牌
                (lastValidCombo.comboType == CardComboType::PASS && myAction.comboType != CardComboType::INVALID)
            );
    }
    catch (exception& e)
    {
        myAction = lastValidCombo.findFirstValid(myCards.begin(), myCards.end());
    }
 
 
    IO.play(myAction.cards.begin(), myAction.cards.end());
    }
}