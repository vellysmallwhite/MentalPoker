#include "CryptoUtils.h"
#include <algorithm>
#include <chrono>
#include <random>
#include <iostream>
#include <ctime>
#include <fstream>
#include <stdexcept>

// Helper function to compute GCD
int gcd(int a, int b) {
    while (b != 0) {
        int temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}

void readSharedModulus(mpz_class& p, mpz_class& q, mpz_class& n, mpz_class& phi_n) {
    std::ifstream keyFile("/src/key");  // Use the correct path
    if (!keyFile) {
        throw std::runtime_error("Failed to open key file at /src/key.");
    }

    std::string p_str, q_str, n_str;
    if (!std::getline(keyFile, p_str) ||
        !std::getline(keyFile, q_str) ||
        !std::getline(keyFile, n_str)) {
        throw std::runtime_error("Failed to read p, q, n from key file.");
    }

    p = mpz_class(p_str);
    q = mpz_class(q_str);
    n = mpz_class(n_str);

    // Calculate phi_n = (p - 1) * (q - 1)
    phi_n = (p - 1) * (q - 1);
}

void generateSRAKeyPair(const mpz_class& n, const mpz_class& phi_n, KeyPair& keyPair) {
    // Initialize random number generator
    gmp_randclass randGen(gmp_randinit_mt);
    randGen.seed(static_cast<unsigned long>(std::time(nullptr)));

    // Choose e such that 1 < e < phi_n and gcd(e, phi_n) == 1
    mpz_class e;
    mpz_class gcd;
    do {
        e = randGen.get_z_range(phi_n - 2) + 200000000; // Range: [2, phi_n - 1]
        mpz_gcd(gcd.get_mpz_t(), e.get_mpz_t(), phi_n.get_mpz_t());
    } while (mpz_cmp_ui(gcd.get_mpz_t(), 1) != 0);

    // Compute d, the modular inverse of e modulo phi_n
    mpz_class d;
    if (mpz_invert(d.get_mpz_t(), e.get_mpz_t(), phi_n.get_mpz_t()) == 0) {
        throw std::runtime_error("Failed to compute modular inverse for key generation.");
    }

    // Ensure e and d are not the same
    if (e == d) {
        throw std::runtime_error("Encryption and decryption keys are the same. Regenerate keys.");
    }

    // Set the key pair
    keyPair.publicKey = e;
    keyPair.privateKey = d;
    keyPair.n = n;
}

mpz_class SRAEncrypt(const mpz_class& message, const mpz_class& key, const mpz_class& n) {
    mpz_class ciphertext;
    mpz_powm(ciphertext.get_mpz_t(), message.get_mpz_t(), key.get_mpz_t(), n.get_mpz_t());
    return ciphertext;
}

mpz_class SRADecrypt(const mpz_class& ciphertext, const mpz_class& key, const mpz_class& n) {
    mpz_class message;
    mpz_powm(message.get_mpz_t(), ciphertext.get_mpz_t(), key.get_mpz_t(), n.get_mpz_t());
    return message;
}

// Generate a standard deck of 52 cards
std::vector<Card> generateDeck() {
    std::vector<Card> deck;
    const std::string suits[] = {"Hearts", "Diamonds", "Clubs", "Spades"};
    for (const auto& suit : suits) {
        for (int rank = 1; rank <= 13; ++rank) {
            deck.push_back({rank, suit});
        }
    }
    return deck;
}








// Shuffle the encoded deck
void shuffleDeck(EncodedDeck& deck) {
    std::shuffle(deck.begin(), deck.end(), std::mt19937{std::random_device{}()});
}

int cardToCardNumber(const Card& card) {
    int suitValue = 0;
    if (card.suit == "Hearts") suitValue = 0;
    else if (card.suit == "Diamonds") suitValue = 1;
    else if (card.suit == "Clubs") suitValue = 2;
    else if (card.suit == "Spades") suitValue = 3;

    // Unique card number from 1 to 52
    int cardNumber = suitValue * 13 + card.rank;
    return cardNumber;
}

Card cardNumberToCard(int cardNumber) {
    int suitValue = (cardNumber - 1) / 13;
    int rank = ((cardNumber - 1) % 13) + 1;

    std::string suit;
    switch (suitValue) {
        case 0: suit = "Hearts"; break;
        case 1: suit = "Diamonds"; break;
        case 2: suit = "Clubs"; break;
        case 3: suit = "Spades"; break;
        default: suit = "Unknown"; break;
    }
    return {rank, suit};
}

mpz_class encodeCardValue(int cardNumber) {
    // Simple encoding by adding a large constant to cardNumber
    mpz_class encodedValue = cardNumber + mpz_class("100000000000000000000000000000000000000");
    return encodedValue;
}

int decodeCardValue(const mpz_class& encodedValue) {
    mpz_class cardNumber = encodedValue - mpz_class("100000000000000000000000000000000000000");
    return cardNumber.get_ui();
}

// Encode the entire deck
void encodeDeck(const std::vector<Card>& deck, EncodedDeck& encodedDeck) {
    encodedDeck.clear();
    for (const auto& card : deck) {
        int cardNumber = cardToCardNumber(card);
        mpz_class encodedValue = encodeCardValue(cardNumber);
        encodedDeck.push_back(encodedValue);
    }
}

// Encrypt the entire encoded deck
void encryptDeck(const EncodedDeck& encodedDeck, EncodedDeck& encryptedDeck, const mpz_class& publicKey, const mpz_class& n) {
    encryptedDeck.clear();
    for (const auto& encodedValue : encodedDeck) {
        mpz_class encryptedValue = SRAEncrypt(encodedValue, publicKey, n);
        encryptedDeck.push_back(encryptedValue);
    }
}

// Decrypt the entire encrypted deck
void decryptDeck(const EncodedDeck& encryptedDeck, EncodedDeck& decryptedDeck, const mpz_class& privateKey, const mpz_class& n) {
    decryptedDeck.clear();
    for (const auto& encryptedValue : encryptedDeck) {
        mpz_class decryptedValue = SRADecrypt(encryptedValue, privateKey, n);
        decryptedDeck.push_back(decryptedValue);
    }
}

// Convert encoded values to Cards
void decodeDeck(const EncodedDeck& decryptedDeck, std::vector<Card>& deck) {
    deck.clear();
    for (const auto& decryptedValue : decryptedDeck) {
        int cardNumber = decodeCardValue(decryptedValue);
        Card card = cardNumberToCard(cardNumber);
        deck.push_back(card);
    }
}


void printEncodedDeck(const EncodedDeck& deck) {
    std::cout << "Encoded Deck:" ;
    for (size_t i = 0; i < deck.size(); ++i) {
        std::cout << "Index " << i << ": " << deck[i].get_str() <<" || ";
    }
    std::cout <<std::endl;

}




// Display a card (for debugging)
std::string cardToString(const Card& card) {
    std::string rankStr;
    switch (card.rank) {
        case 1:  rankStr = "Ace"; break;
        case 11: rankStr = "Jack"; break;
        case 12: rankStr = "Queen"; break;
        case 13: rankStr = "King"; break;
        default: rankStr = std::to_string(card.rank); break;
    }
    return rankStr + " of " + card.suit;
}