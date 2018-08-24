// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

<template>
<div style="height: 100%">
  <v-toolbar dark dense>
    <v-btn icon @click.stop.native="back">
      <v-icon>arrow_back</v-icon>
    </v-btn>
    <v-spacer></v-spacer>
  </v-toolbar>

  <v-tabs dark v-model="active" style="height: 0px;">
  </v-tabs>
  <v-tabs-items v-model="active">

    <v-tab-item key="0" style="height: 100%">
      <v-container fluid style="height: 100%">
        <v-card>
          <v-card-title primary-title>
            <div>
              <h3 class="headline mb-0">Sign in to NoctuaSky</h3>
            </div>
          </v-card-title>
          <v-progress-circular v-if="signInInProgress" indeterminate v-bind:size="50" style="position: absolute; left: 0; right: 0; margin-left: auto; margin-right: auto; margin-top: 60px;"></v-progress-circular>
          <v-card-text>
            <form action="">
              <v-text-field :disabled="signInInProgress" name="input-login" label="Email" autocomplete="username" v-model="email" @input="clearSignInErrorAlert"></v-text-field>
              <v-text-field :disabled="signInInProgress" name="input-pwd" label="Password" autocomplete="password" v-model="password" type="password" @keyup.native.enter="signIn()" @input="clearSignInErrorAlert"></v-text-field>
              <v-alert error v-model="showSignInErrorAlert">{{ signInErrorAlert }}</v-alert>
              <v-layout row justify-space-around>
                <v-btn @click.stop.native="signIn()" :disabled="signInButtonDisabled || signInInProgress">Sign In</v-btn>
              </v-layout>
            </form>
          </v-card-text>
          <v-card-actions>
          </v-card-actions>
        </v-card>
        <v-layout row justify-space-around style="margin-top: 30px">
          <p>New to NoctuaSky? <v-btn flat small @click.stop.native="showSignupTab()">Create an account</v-btn></p>
        </v-layout>
      </v-container>
    </v-tab-item>

    <v-tab-item key="1" style="height: 100%">
      <v-container fluid style="height: 100%">
        <v-card>
          <v-card-title primary-title>
            <div>
              <h3 class="headline mb-0">Create a NoctuaSky account</h3>
            </div>
          </v-card-title>
          <v-progress-circular v-if="signUpInProgress" indeterminate v-bind:size="50" style="position: absolute; left: 0; right: 0; margin-left: auto; margin-right: auto; margin-top: 60px;"></v-progress-circular>
          <v-card-text>
            <v-layout row justify-space-between>
              <v-text-field name="input-first-name" label="First Name" v-model="signUpFirstName"></v-text-field>
              &nbsp;
              <v-text-field name="input-last-name" label="Last Name" v-model="signUpLastName"></v-text-field>
            </v-layout>
            <form action="">
            <v-text-field required name="input-email" label="Email" autocomplete="username" v-model="signUpEmail" :rules="[rules.email]"></v-text-field>
            <v-text-field required name="input-pwd" label="Create password" autocomplete="new-password" v-model="signUpPassword" type="password" counter :rules="[rules.password]"></v-text-field>
            <v-text-field required name="input-pwd2" label="Confirm password" autocomplete="new-password" v-model="signUpPassword2" type="password" :rules="[rules.confirmPassword]"></v-text-field>
            </form>
            <v-alert error v-model="showSignUpErrorAlert">{{ signUpErrorAlert }}</v-alert>
            <v-layout row justify-space-around>
              <v-btn @click.stop.native="signUp()" :disabled="formValid">Sign Up</v-btn>
            </v-layout>
          </v-card-text>
        </v-card>
      </v-container>
    </v-tab-item>

    <v-tab-item key="2" style="height: 100%">
      <v-container fluid style="height: 100%">
        <v-card>
          <v-card-title primary-title>
            <div>
              <h3 class="headline mb-0">Thanks for Signin In to NoctuaSky.com!</h3>
            </div>
          </v-card-title>
          <v-card-text>
            <p>Before you can continue, you need to confirm that your email is valid.</p>
            <p>We have just sent you a message containing a confirmation link that you need to click.</p>
          </v-card-text>
        </v-card>
        <v-layout row justify-space-around style="margin-top: 30px">
          <p><v-btn flat small @click.stop.native="showSigninTab()">OK, get me to SignIn</v-btn></p>
        </v-layout>
      </v-container>
    </v-tab-item>

  </v-tabs-items>
</div>
</template>

<script>

import NoctuaSkyClient from '@/assets/noctuasky-client'

export default {
  data: function () {
    return {
      email: '',
      password: '',
      signInErrorAlert: '',
      signUpFirstName: '',
      signUpLastName: '',
      signUpPassword: '',
      signUpPassword2: '',
      signUpEmail: '',
      signUpErrorAlert: '',
      active: '0',

      rules: {
        email: (value) => {
          if (value === '') {
            return true
          }
          const pattern = /^(([^<>()[\]\\.,;:\s@"]+(\.[^<>()[\]\\.,;:\s@"]+)*)|(".+"))@((\[[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}])|(([a-zA-Z\-0-9]+\.)+[a-zA-Z]{2,}))$/
          return pattern.test(value) || 'Invalid e-mail.'
        },
        password: (value) => {
          if (value === '') {
            return true
          }
          if (value.length < 8) {
            return 'Use at least 8 characters'
          }
          return true
        },
        confirmPassword: (value) => {
          if (value === '') {
            return true
          }
          if (value !== this.signUpPassword) {
            return 'Passwords don\'t match'
          }
          return true
        }
      }
    }
  },
  methods: {
    signIn: function () {
      var that = this
      NoctuaSkyClient.users.login(this.email, this.password).then(function (res) { that.back() }, function (res) {
        if (res.message) {
          that.signInErrorAlert = res.message
        } else if (res.errors) {
          that.signInErrorAlert = ''
          for (let k in res.errors) {
            that.signInErrorAlert += k + ': ' + res.errors[k] + '\n'
          }
        }
      })
    },
    signUp: function () {
      var that = this
      NoctuaSkyClient.users.register(this.signUpEmail, this.signUpPassword, this.signUpFirstName, this.signUpLastName).then(res => {
        that.showWaitConfirmationEmailTab()
      }).catch(function (res) {
        if (res.message) {
          that.signUpErrorAlert = res.message
        } else if (res.errors) {
          that.signUpErrorAlert = ''
          for (let k in res.errors) {
            that.signUpErrorAlert += k + ': ' + res.errors[k] + '\n'
          }
        }
      })
    },
    showWaitConfirmationEmailTab: function () {
      this.email = this.signUpEmail
      this.password = this.signUpPassword
      this.active = '2'
    },
    showSignupTab: function () {
      this.active = '1'
    },
    showSigninTab: function () {
      this.active = '0'
    },
    clearSignInErrorAlert: function () {
      this.signInErrorAlert = ''
    },
    clearSignUpErrorAlert: function () {
      this.signUpErrorAlert = ''
    },
    back: function () {
      if (this.active === '1' || this.active === '2') {
        this.active = '0'
        return
      }
      this.$router.go(-1)
    }
  },
  computed: {
    formValid: function () {
      return this.signUpUserName !== '' && this.signUpPassword.len > 5 && this.signUpPassword === this.signUpPassword2
    },
    signInButtonDisabled: function () {
      return this.email === '' || this.password === ''
    },
    signInInProgress: function () {
      return this.$store.state.plugins.observing.noctuaSky.status === 'signInInProgress'
    },
    signUpInProgress: function () {
      return this.$store.state.plugins.observing.noctuaSky.status === 'signUpInProgress'
    },
    showSignInErrorAlert: function () {
      return this.signInErrorAlert !== ''
    },
    showSignUpErrorAlert: function () {
      return this.signUpErrorAlert !== ''
    }
  }
}
</script>

<style>
.tabs {
  height: 100%;
}
.tabs__items {
  height: 100%;
  border-style: none;
}
.tabs__bar {
  background-color: #303030;
  opacity: 0;
}
</style>
