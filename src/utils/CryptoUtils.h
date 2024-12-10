#pragma once

#include <vector>
#include <string>
#include <gmpxx.h>

// Encoded Deck structure
typedef std::vector<mpz_class> EncodedDeck;

// Card structure
struct Card {
    int rank;           // 1-13 (Ace to King)
    std::string suit;   // "Hearts", "Diamonds", "Clubs", "Spades"
};

// Key Pair structure
struct KeyPair {
    mpz_class publicKey;    // Encryption key (e)
    mpz_class privateKey;   // Decryption key (d)
    mpz_class n;            // Modulus
};

// Encrypted Player Hand structure
struct EncryptedPlayerHand {
    std::vector<mpz_class> encryptedCards;
};

// Player Hand structure
struct PlayerHand {
    std::vector<Card> cards;
};

// Generate shared modulus n
void readSharedModulus(mpz_class& p, mpz_class& q, mpz_class& n, mpz_class& phi_n);

// Generate SRA Key Pair with shared modulus
void generateSRAKeyPair(const mpz_class& n, const mpz_class& phi_n, KeyPair& keyPair);

// SRA Encryption and Decryption functions
mpz_class SRAEncrypt(const mpz_class& message, const mpz_class& key, const mpz_class& n);
mpz_class SRADecrypt(const mpz_class& ciphertext, const mpz_class& key, const mpz_class& n);

// Generate a standard deck of 52 cards
std::vector<Card> generateDeck();

// Shuffle the encoded deck
void shuffleDeck(EncodedDeck& deck);

// Encode and decode card values
mpz_class encodeCardValue(int cardNumber);
int decodeCardValue(const mpz_class& encodedValue);

// Map card number to Card and vice versa
Card cardNumberToCard(int cardNumber);
int cardToCardNumber(const Card& card);

// Encode the entire deck
void encodeDeck(const std::vector<Card>& deck, EncodedDeck& encodedDeck);

// Encrypt the entire encoded deck
void encryptDeck(const EncodedDeck& encodedDeck, EncodedDeck& encryptedDeck, const mpz_class& publicKey, const mpz_class& n);

// Decrypt the entire encrypted deck
void decryptDeck(const EncodedDeck& encryptedDeck, EncodedDeck& decryptedDeck, const mpz_class& privateKey, const mpz_class& n);

// Convert encoded values to Cards
void decodeDeck(const EncodedDeck& decryptedDeck, std::vector<Card>& deck);

// Display a card as a string
std::string cardToString(const Card& card);

void printEncodedDeck(const EncodedDeck& deck);